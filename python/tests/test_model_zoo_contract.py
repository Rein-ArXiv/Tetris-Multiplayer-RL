"""Regression boundaries for comparing the hand-written bot trainers."""
from types import SimpleNamespace
import numpy as np
import pytest
torch = pytest.importorskip("torch")
from common import BOARD_ROWS as H, BOARD_COLS as W, NUM_PIECE_TYPES as K, NUM_PLACEMENTS as A
from train import dqn_tetris as dqn, muzero_tetris as muzero
from train.rl_common import ReplayBuffer

torch.set_num_threads(1)


def observation(value=0.):
    return dict(board=np.full((1,H,W),value,np.float32),current=np.eye(K,dtype=np.float32)[0],
                next=np.eye(K,dtype=np.float32)[0])


def mask():
    result=np.zeros(A,bool);result[:2]=True
    return result


class QProbe(torch.nn.Module):
    def __init__(self):
        super().__init__();self.weight=torch.nn.Parameter(torch.ones(A))
    def forward(self,board,current,next_piece):
        return self.weight.expand(len(board),-1),torch.zeros(len(board))


@pytest.mark.parametrize('mode',['dqn','ddqn'])
def test_terminal_empty_mask_has_finite_reward_target(mode):
    replay=ReplayBuffer(1); replay.add(observation(),mask(),0,3.,True,observation(),np.zeros(A,bool))
    model=QProbe(); target=QProbe(); opt=torch.optim.SGD(model.parameters(),lr=0.)
    loss=dqn.train_step(model,target,opt,replay,batch_size=1,gamma=.5,target_mode=mode,
                        device=torch.device('cpu'),max_grad_norm=1.)
    assert loss==pytest.approx(1.5) # smooth_l1(1,3)
    assert torch.isfinite(model.weight.grad).all()


def test_live_empty_mask_is_a_contract_error():
    replay=ReplayBuffer(1); replay.add(observation(),mask(),0,3.,False,observation(),np.zeros(A,bool))
    model=QProbe();opt=torch.optim.SGD(model.parameters(),lr=0.)
    with pytest.raises(ValueError,match='nonterminal'):
        dqn.train_step(model,QProbe(),opt,replay,batch_size=1,gamma=.5,target_mode='ddqn',
                       device=torch.device('cpu'),max_grad_norm=1.)


class BoundaryEnv:
    def __init__(self,terminal=False,budget=False):self.terminal=terminal;self.budget=budget;self.closed=False
    def reset(self,seed=None):return observation(99.),dict(legal_mask=mask(),lines=0,score=0)
    def step(self,a):
        return observation(5.),2.,self.terminal,not (self.terminal or self.budget),dict(legal_mask=mask(),lines=2,score=7)
    def close(self):self.closed=True


def test_dqn_truncation_resets_but_keeps_bootstrap(monkeypatch,tmp_path):
    env=BoundaryEnv();monkeypatch.setattr(dqn,'TetrisPlacementEnv',lambda **k:env)
    from common.models import TetrisPolicyNet
    monkeypatch.setattr(dqn,'TetrisPolicyNet',lambda:TetrisPolicyNet(conv_channels=(2,),hidden=4))
    captured=[]
    def capture(*args,**kwargs):
        captured.append(bool(args[3].done[0]));return 0.
    monkeypatch.setattr(dqn,'train_step',capture)
    args=dqn.build_argparser().parse_args(['--steps','1','--warmup','1','--shaping-coef','0',
          '--out',str(tmp_path/'dqn.pt'),'--device','cpu'])
    dqn.train(args)
    assert captured==[False]
    assert env.closed


class ValueNet(torch.nn.Module):
    def represent(self,board,current,next_piece):return board[:,0,0,:1]
    def predict(self,latent):return torch.zeros(len(latent),A),latent[:,0]


@pytest.mark.parametrize('terminal,budget,expected',[(True,False,1.),(False,False,3.5),(False,True,3.5)])
def test_muzero_bootstraps_real_endpoint(monkeypatch,terminal,budget,expected):
    env=BoundaryEnv(terminal,budget);monkeypatch.setattr(muzero,'TetrisPlacementEnv',lambda **k:env)
    policy=np.zeros(A,np.float32);policy[0]=1
    monkeypatch.setattr(muzero,'run_mcts',lambda *a,**k:(0,policy))
    args=muzero.build_argparser().parse_args(['--max-pieces','1','--gamma','.5','--value-scale','2','--reward-scale','2'])
    replay=ReplayBuffer(1)
    lines,pieces=muzero.self_play_episode(ValueNet(),replay,args,episode=0,device=torch.device('cpu'))
    assert replay.value_target[0]==pytest.approx(expected)
    assert bool(replay.done[0])==terminal
    assert env.closed and (lines,pieces)==(2.,1)


def test_muzero_rejects_mixed_value_reward_units(tmp_path):
    args=muzero.build_argparser().parse_args(['--episodes','1','--value-scale','2','--reward-scale','3',
                '--out',str(tmp_path/'unused.pt')])
    with pytest.raises(ValueError,match='same'):
        muzero.train(args)


def test_missing_dqn_warm_start_does_not_start_environment(monkeypatch,tmp_path):
    monkeypatch.setattr(dqn,'TetrisPlacementEnv',lambda **k:pytest.fail('env before checkpoint'))
    args=dqn.build_argparser().parse_args(['--resume',str(tmp_path/'absent.pt')])
    with pytest.raises(FileNotFoundError):dqn.train(args)


def test_visit_temperature_stays_finite():
    p=muzero.visit_policy(np.array([10000.,9999.,0.]),np.array([True,True,False]),.001)
    assert np.isfinite(p).all() and p.sum()==pytest.approx(1) and p[2]==0


def test_shared_evaluation_keeps_rows_and_restores_on_failure():
    from train.rl_common import evaluate_episodes
    model=QProbe().train();env=BoundaryEnv()
    rows=evaluate_episodes(model,seeds=[10,20],device='cpu',max_pieces=3,make_env=lambda:env)
    assert [r['seed'] for r in rows]==[10,20]
    assert rows[0]['lines']==2 and rows[0]['end']=='truncated'
    assert env.closed and model.training
    class Failing(BoundaryEnv):
        def step(self,a):raise RuntimeError('injected')
    failed=Failing()
    with pytest.raises(RuntimeError,match='injected'):
        evaluate_episodes(model,seeds=[10],device='cpu',max_pieces=3,make_env=lambda:failed)
    assert failed.closed and model.training


def test_muzero_checkpoint_checks_class_scale_and_atomic_save(tmp_path,monkeypatch):
    args=muzero.build_argparser().parse_args([])
    net=muzero.MuZeroNet(hidden=4,latent=4)
    path=tmp_path/'native.pt';muzero.save_muzero_checkpoint(net,path,args,episodes=1)
    restored=muzero.load_muzero_checkpoint(path,torch.device('cpu'),value_scale=args.value_scale)
    assert all(torch.equal(v,restored.state_dict()[k]) for k,v in net.state_dict().items())
    with pytest.raises(ValueError,match='differs'):
        muzero.load_muzero_checkpoint(path,torch.device('cpu'),value_scale=2.)
    saved=path.read_bytes()
    import common.atomic_save as atomic
    def fail(*a,**k):raise OSError('injected save')
    monkeypatch.setattr(atomic.os,'replace',fail)
    with pytest.raises(OSError):muzero.save_muzero_checkpoint(net,path,args,episodes=2)
    assert path.read_bytes()==saved
    payload=torch.load(path,weights_only=True);payload['__meta__']['class']='Other'
    torch.save(payload,path)
    with pytest.raises(ValueError,match='MuZeroNet'):
        muzero.load_muzero_checkpoint(path,torch.device('cpu'))


@pytest.mark.parametrize('module_name',['cem_tetris','cbmpi_tetris','muzero_tetris'])
def test_other_missing_warm_start_is_not_silent(module_name,tmp_path):
    import importlib
    module=importlib.import_module('train.'+module_name)
    args=module.build_argparser().parse_args(['--resume',str(tmp_path/'missing.pt'),'--device','cpu'])
    with pytest.raises(FileNotFoundError):module.train(args)


@pytest.mark.parametrize('mode,target_value',[('ddqn',3.),('dqn',9.)])
def test_double_dqn_separates_selection_from_evaluation(mode,target_value):
    replay=ReplayBuffer(1);replay.add(observation(),mask(),0,2.,False,observation(),mask())
    online,target=QProbe(),QProbe()
    with torch.no_grad():
        online.weight[:2]=torch.tensor([8.,7.]);target.weight[:2]=torch.tensor([3.,9.])
    opt=torch.optim.SGD(online.parameters(),lr=0.)
    actual=dqn.train_step(online,target,opt,replay,batch_size=1,gamma=.5,target_mode=mode,
                          device=torch.device('cpu'),max_grad_norm=1.)
    expected=torch.nn.functional.smooth_l1_loss(torch.tensor(8.),torch.tensor(2.+.5*target_value)).item()
    assert actual==pytest.approx(expected)


@pytest.mark.parametrize('change',['null_version','boolean_semantic'])
def test_muzero_present_but_invalid_version_is_not_legacy(tmp_path,change):
    path=tmp_path/'model.pt';args=muzero.build_argparser().parse_args([])
    muzero.save_muzero_checkpoint(muzero.MuZeroNet(hidden=4,latent=4),path,args,episodes=0)
    payload=torch.load(path,weights_only=True)
    if change=='null_version':payload['__meta__']['format_version']=None
    else:payload['__meta__']['io_contract']['observation_version']=True
    torch.save(payload,path)
    with pytest.raises(ValueError,match='contract mismatch'):
        muzero.load_muzero_checkpoint(path,torch.device('cpu'))
