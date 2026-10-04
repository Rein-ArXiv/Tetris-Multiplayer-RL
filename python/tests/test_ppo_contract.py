"""Exercise real PPO collection/update with controlled reset boundaries."""
from types import SimpleNamespace
import numpy as np
import pytest
import torch
from common import BOARD_ROWS as H,BOARD_COLS as W,NUM_PIECE_TYPES as K,NUM_PLACEMENTS as A
from common.models import TetrisPolicyNet
from train import ppo_tetris as ppo

torch.set_num_threads(1)

class BoundaryEnv:
    def __init__(self,terminal=False,fail=False):
        self.terminal=terminal;self.fail=fail;self.resets=0;self.steps=0;self.closed=False
    def observation(self,x):
        return {'board':np.full((1,H,W),x,dtype=np.float32),
                'current':np.eye(K,dtype=np.float32)[0],
                'next':np.eye(K,dtype=np.float32)[1]}
    def info(self,lines=0):
        mask=np.zeros(A,dtype=np.bool_);mask[:2]=True
        return {'legal_mask':mask,'lines':lines,'agent_lines':lines,'score':7}
    def reset(self,seed=None):
        self.resets+=1
        return self.observation(100.),self.info()
    def step(self,action):
        if self.fail:raise RuntimeError('fixture failure')
        self.steps+=1
        return self.observation(5.),9.,self.terminal,not self.terminal,self.info(2)
    def close(self):self.closed=True

class ValueProbe(torch.nn.Module):
    n_placements=A
    n_piece_types=K
    def __init__(self):
        super().__init__();self.weight=torch.nn.Parameter(torch.tensor(1.))
    def forward(self,board,current,next):
        value=board[:,0,0,0]*self.weight
        logits=self.weight*torch.zeros(board.shape[0],A)
        return logits,value

def args(tmp_path,**changes):
    a=ppo.build_argparser().parse_args(['--steps','3','--rollout','2','--epochs','1',
        '--minibatch','2','--eval-episodes','0','--shaping-coef','0','--out',str(tmp_path/'run.pt')])
    for k,v in changes.items():setattr(a,k,v)
    return a

def test_evaluation_uses_lines_not_versus_reward(monkeypatch):
    env=BoundaryEnv();monkeypatch.setattr(ppo,'make_env',lambda *a:env)
    net=ValueProbe().train()
    result=ppo.evaluate_policy(net,episodes=2,seed=1,device='cpu',max_pieces=5,env_kind='versus')
    assert result['avg_lines']==2
    assert result['avg_reward']==9
    assert env.closed and net.training

def test_evaluation_restores_mode_and_closes_after_failure(monkeypatch):
    env=BoundaryEnv(fail=True);monkeypatch.setattr(ppo,'make_env',lambda *a:env)
    net=ValueProbe().train()
    with pytest.raises(RuntimeError,match='fixture failure'):
        ppo.evaluate_policy(net,episodes=1,seed=1,device='cpu',max_pieces=5)
    assert net.training and env.closed

def test_exact_budget_and_singleton_update_stay_finite(monkeypatch,tmp_path):
    env=BoundaryEnv();net=TetrisPolicyNet(conv_channels=(2,),hidden=8)
    monkeypatch.setattr(ppo,'make_env',lambda *a:env)
    monkeypatch.setattr(ppo,'TetrisPolicyNet',lambda:net)
    saved=[];monkeypatch.setattr(ppo,'save_checkpoint',lambda m,p,extra=None:saved.append(extra))
    ppo.train(args(tmp_path))
    assert env.steps==3 and saved[-1]['training_steps']==3
    assert all(torch.isfinite(p).all() for p in net.parameters())

def test_single_transition_training_stays_finite(monkeypatch,tmp_path):
    env=BoundaryEnv();net=TetrisPolicyNet(conv_channels=(2,),hidden=8)
    monkeypatch.setattr(ppo,'make_env',lambda *a:env)
    monkeypatch.setattr(ppo,'TetrisPolicyNet',lambda:net)
    monkeypatch.setattr(ppo,'save_checkpoint',lambda *a,**kw:None)
    ppo.train(args(tmp_path,steps=1,rollout=1,minibatch=1))
    assert all(torch.isfinite(p).all() for p in net.parameters())

@pytest.mark.parametrize('terminal',[False,True])
def test_collector_bootstraps_endpoint_before_reset(monkeypatch,tmp_path,terminal):
    env=BoundaryEnv(terminal=terminal);net=ValueProbe()
    monkeypatch.setattr(ppo,'make_env',lambda *a:env)
    monkeypatch.setattr(ppo,'TetrisPolicyNet',lambda:net)
    monkeypatch.setattr(ppo,'save_checkpoint',lambda *a,**kw:None)
    actual=ppo.gae_targets;seen=[]
    def capture(*values):
        result=actual(*values)
        seen.append(([x.clone() if isinstance(x,torch.Tensor) else x for x in values],result))
        return result
    monkeypatch.setattr(ppo,'gae_targets',capture)
    ppo.train(args(tmp_path,steps=2,rollout=2,gamma=.9,lam=.8,lr=1e-9))
    inputs,(_,targets)=seen[0]
    assert inputs[5].all()
    torch.testing.assert_close(inputs[2],torch.full((2,),0. if terminal else 5.))
    torch.testing.assert_close(targets,torch.full((2,),9. if terminal else 13.5))

@pytest.mark.parametrize('name,value',[('rollout',0),('epochs',0),('minibatch',0),('steps',-1),('gamma',float('nan')),('lam',1.1),('lr',0),('max_grad_norm',0),('save_every',-1)])
def test_invalid_training_settings_rejected_before_env(monkeypatch,tmp_path,name,value):
    monkeypatch.setattr(ppo,'make_env',lambda *a:pytest.fail('must reject before creating environment'))
    with pytest.raises((TypeError,ValueError)):ppo.train(args(tmp_path,**{name:value}))

@pytest.mark.parametrize('terminal',[False,True])
def test_a2c_does_not_join_reset_episodes(terminal):
    from train.policy_gradient_tetris import collect_a2c_rollout
    env=BoundaryEnv(terminal=terminal);obs,info=env.reset();net=ValueProbe()
    settings=SimpleNamespace(rollout=3,max_pieces=10,gamma=.9,shaping_coef=0.,temperature=1.)
    data,_,_,lines,lengths=collect_a2c_rollout(net,env,obs,info,settings,device='cpu')
    assert data['return']==pytest.approx([9. if terminal else 13.5]*3)
    assert lines==[2.]*3 and lengths==[1]*3

def test_a2c_episode_limit_persists_across_rollouts():
    from train.policy_gradient_tetris import collect_a2c_rollout
    class Continuing(BoundaryEnv):
        def step(self,action):
            obs,reward,_,_,info=super().step(action)
            return obs,reward,False,False,info
    env=Continuing();obs,info=env.reset();net=ValueProbe();progress={}
    settings=SimpleNamespace(rollout=2,max_pieces=3,gamma=.9,shaping_coef=0.,temperature=1.)
    _,obs,info,_,lens=collect_a2c_rollout(net,env,obs,info,settings,device='cpu',progress=progress)
    assert lens==[] and progress['decisions']==2
    _,_,_,_,lens=collect_a2c_rollout(net,env,obs,info,settings,device='cpu',progress=progress)
    assert lens==[3] and progress['decisions']==1 and env.resets==2

@pytest.mark.parametrize('terminal',[False,True])
def test_episode_return_uses_value_tail_only_at_cutoff(terminal):
    from train.policy_gradient_tetris import collect_reinforce_episode
    settings=SimpleNamespace(max_pieces=5,gamma=.9,shaping_coef=0.,temperature=1.)
    data,lines,count=collect_reinforce_episode(ValueProbe(),BoundaryEnv(terminal),settings,device='cpu')
    assert data['return']==pytest.approx([9. if terminal else 13.5])
    assert lines==2 and count==1

def test_actor_critic_update_matches_sampling_temperature(monkeypatch):
    from train import policy_gradient_tetris as pg
    class Unequal(ValueProbe):
        def forward(self,board,current,next):
            logits=self.weight*torch.arange(A,dtype=torch.float32)[None,:].expand(len(board),-1)
            return logits,board[:,0,0,0]*self.weight
    model=Unequal();env=BoundaryEnv();obs,info=env.reset();seen=[]
    actual=pg._masked_logp_entropy
    def capture(logits,mask):seen.append(logits.detach().clone());return actual(logits,mask)
    monkeypatch.setattr(pg,'_masked_logp_entropy',capture)
    pg.sample_action(model,obs,info['legal_mask'],device='cpu',temperature=2.)
    data={k:[] for k in ('board','current','next','mask','action')}
    pg.append_transition(data,obs,info['legal_mask'],0)
    data['return']=[1.];data['advantage']=[1.]
    settings=SimpleNamespace(epochs=1,batch=1,temperature=2.,normalize_advantage=False,value_coef=.5,entropy_coef=.01,max_grad_norm=.5)
    pg.update_policy(model,torch.optim.SGD(model.parameters(),lr=.001),data,settings,device='cpu')
    torch.testing.assert_close(seen[0],seen[1])
