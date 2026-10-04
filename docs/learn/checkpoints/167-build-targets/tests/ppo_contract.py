import argparse
import copy
from pathlib import Path
import sys
import unittest
import torch
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
import study_py as sim
from gym_env import RoundEnv
from rewards import RewardSpec
from policy_network import PolicySchema,PolicyNet
from ppo import Collector,update,evaluate,batch_of
from returns import gae_targets,normalize_advantages
from ppo_loss import clipped_policy_loss

torch.set_num_threads(1)

class RecordedEnv(RoundEnv):
 def __init__(self,**kwargs):
  super().__init__(**kwargs);self.endpoints=[]
 def step(self,action):
  result=super().step(action);self.endpoints.append(copy.deepcopy(result[0]));return result

class PPOContract(unittest.TestCase):
 def model(self):
  torch.manual_seed(157)
  return PolicyNet(PolicySchema.from_native(sim.observation_schema(),sim.action_schema()))

 def test_two_masks_and_rollout_cutoff(self):
  f=lambda x:torch.tensor(x,dtype=torch.float64)
  b=lambda x:torch.tensor(x,dtype=torch.bool)
  a,r=gae_targets(f([1,2,3]),f([10,20,30]),f([20,7,40]),f([.9,.9,.9]),b([0,0,1]),b([0,1,1]),.8)
  torch.testing.assert_close(r,f([10.576,8.3,3]))
  torch.testing.assert_close(normalize_advantages(f([-2])),f([-2]))
  with self.assertRaises(ValueError):gae_targets(f([1]),f([0]),f([0]),f([.9]),b([1]),b([0]),.95)

 def test_clipping_keeps_bad_direction_gradient(self):
  for ratio,adv,expected,flat in [(1.5,1,-1.2,True),(.5,-1,.8,True),(1.5,-1,1.5,False),(.5,1,-.5,False)]:
   x=torch.tensor([ratio],dtype=torch.float64).log().requires_grad_()
   loss,kl,fraction=clipped_policy_loss(x,torch.zeros_like(x),torch.full_like(x,adv),.2)
   self.assertAlmostEqual(float(loss.detach()),expected);loss.backward()
   self.assertEqual(float(x.grad)==0,flat)
   self.assertGreaterEqual(float(kl),0);self.assertEqual(float(fraction),1)

 def test_actual_endpoint_frozen_targets_and_update(self):
  model=self.model().eval();env=RecordedEnv(max_steps=2,reward_spec=RewardSpec(gamma=.9))
  collector=Collector(env,seed=11,fallback_discount=.9)
  data=collector.collect(model,5,device='cpu',lam=.95)
  self.assertEqual(data['boundary'].tolist(),[False,True,False,True,False])
  self.assertFalse(data['terminated'].any())
  with torch.no_grad():
   for i,obs in enumerate(env.endpoints):
    batch=batch_of(obs,'cpu');expected=model(batch['board'],batch['current'],batch['next'])[1][0]
    torch.testing.assert_close(data['next_value'][i],expected)
  old={k:v.clone() for k,v in data.items()}
  params={k:v.detach().clone() for k,v in model.named_parameters()}
  opt=torch.optim.Adam(model.parameters(),lr=.001)
  metrics=update(model,opt,data,epochs=2,minibatch=2)
  self.assertTrue(all(torch.isfinite(torch.tensor(v)) for v in metrics.values()))
  for key,value in data.items():
   self.assertFalse(value.requires_grad);torch.testing.assert_close(value,old[key])
  self.assertTrue(any(not torch.equal(v,params[k]) for k,v in model.named_parameters()))
  single=collector.collect(model,1,device='cpu',lam=.95)
  update(model,opt,single,epochs=1,minibatch=1)
  self.assertTrue(all(torch.isfinite(p).all() for p in model.parameters()))
  env.close()

 def test_evaluation_is_separate_and_restores_mode(self):
  model=self.model().train();env=RoundEnv(max_steps=2)
  collector=Collector(env,seed=1,fallback_discount=.99)
  before=env._session.state_bytes();params={k:v.detach().clone() for k,v in model.named_parameters()}
  make=lambda:RoundEnv(reward_spec=RewardSpec(gamma=.99))
  results=evaluate(model,make,seeds=(5,7),limit=5,device='cpu')
  again=evaluate(model,make,seeds=(5,7),limit=5,device='cpu')
  self.assertEqual(results,again);self.assertTrue(model.training)
  self.assertEqual(before,env._session.state_bytes())
  self.assertEqual(len(results),2)
  for k,p in model.named_parameters():torch.testing.assert_close(p,params[k],rtol=0,atol=0)
  self.assertTrue(all(r['decisions']<=5 and r['lines']>=0 for r in results))
  env.close()

if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
