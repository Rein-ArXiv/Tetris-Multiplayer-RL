import argparse
from pathlib import Path
import sys
import unittest
import numpy as np
import torch
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
import study_py as sim
from observation import observe,stack_batch,to_tensors
from actions import legal_mask,masked_log_softmax,entropy
from policy_network import PolicySchema,PolicyNet

torch.set_num_threads(1)

class PolicyContract(unittest.TestCase):
 def setUp(self):
  torch.manual_seed(156)
  self.obs,self.action=sim.observation_schema(),sim.action_schema()
  self.schema=PolicySchema.from_native(self.obs,self.action)
  self.sessions=[sim.Session(1),sim.Session(11)]
  data=to_tensors(stack_batch(observe(s,self.obs) for s in self.sessions))
  self.batch=tuple(data[k] for k in ('board','current','next'))
  self.mask=torch.from_numpy(np.stack([legal_mask(s,self.action) for s in self.sessions]))
  self.net=PolicyNet(self.schema)

 def test_shapes_batch_independence_and_native_selection(self):
  logits,value=self.net(*self.batch)
  self.assertEqual(tuple(logits.shape),(len(self.sessions),self.schema.actions))
  self.assertEqual(tuple(value.shape),(len(self.sessions),))
  for i,session in enumerate(self.sessions):
   one=self.net(*(x[i:i+1] for x in self.batch))
   torch.testing.assert_close(one[0],logits[i:i+1])
   torch.testing.assert_close(one[1],value[i:i+1])
   logp=masked_log_softmax(one[0],self.mask[i:i+1])
   self.assertTrue(torch.equal(logp.exp()[~self.mask[i:i+1]],torch.zeros_like(logp[~self.mask[i:i+1]])))
   torch.testing.assert_close(logp.exp().sum(-1),torch.ones(1))
   chosen=int(logp.argmax(-1)[0]);self.assertIn(chosen,session.legal_actions())
   before=session.state_bytes();event=session.apply_action(chosen)
   self.assertGreater(event['ticks'],0);self.assertNotEqual(before,session.state_bytes())

 def test_metadata_guards_and_constructor(self):
  b,c,n=self.batch
  for values in [(b.transpose(-1,-2),c,n),(b,c[:,:-1],torch.cat((n,n[:,:1]),-1)),
                 (b[:0],c[:0],n[:0]),(b,c[:1],n),(b[0],c,n),
                 (b.double(),c,n),(b,c.to('meta'),n),([],c,n)]:
   with self.assertRaises((ValueError,TypeError)):self.net(*values)
  for options in ({'hidden':True},{'hidden':0},{'conv_channels':()}, {'conv_channels':(0,)}):
   with self.assertRaises((ValueError,TypeError)):PolicyNet(self.schema,**options)
  with self.assertRaises(ValueError):PolicySchema(0,2,3,4)
  bad=dict(self.action);bad['columns']+=1
  with self.assertRaises(ValueError):PolicySchema.from_native(self.obs,bad)

 def test_two_head_gradients_and_update(self):
  self.net.train();logits,value=self.net(*self.batch)
  logp=masked_log_softmax(logits,self.mask)
  chosen=self.mask.int().argmax(-1)
  loss=-logp.gather(1,chosen[:,None]).mean()+(value-1).square().mean()-.01*entropy(logp,self.mask).mean()
  original={k:v.detach().clone() for k,v in self.net.named_parameters()}
  opt=torch.optim.SGD(self.net.parameters(),lr=.01)
  opt.zero_grad(set_to_none=True);loss.backward()
  for name,param in self.net.named_parameters():
   self.assertIsNotNone(param.grad,name);self.assertTrue(torch.isfinite(param.grad).all(),name)
   torch.testing.assert_close(param,original[name],rtol=0,atol=0)
  for prefix in ('trunk','policy_head','value_head'):
   self.assertTrue(any(p.grad.abs().sum()>0 for n,p in self.net.named_parameters() if n.startswith(prefix)))
  opt.step();self.assertTrue(any(not torch.equal(p,original[n]) for n,p in self.net.named_parameters()))
  self.assertTrue(all(torch.isfinite(p).all() for p in self.net.parameters()))

 def test_eval_no_grad_and_trace_scope(self):
  self.net.eval()
  self.assertTrue(self.net(*self.batch)[1].requires_grad)
  with torch.no_grad():
   self.assertFalse(self.net(*self.batch)[1].requires_grad)
   traced=torch.jit.trace(self.net,self.batch)
   for expected,actual in zip(self.net(*self.batch),traced(*self.batch)):
    torch.testing.assert_close(expected,actual)
  # Same-area transpose proves the trace did not inherit Python shape guards.
  b,c,n=self.batch
  self.assertEqual(traced(b.transpose(-1,-2),c,n)[0].shape,(len(self.sessions),self.schema.actions))
  clone=PolicyNet(self.schema);clone.load_state_dict(self.net.state_dict(),strict=True)
  for x,y in zip(clone(*self.batch),self.net(*self.batch)):torch.testing.assert_close(x,y)

if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
