import pytest
import torch
from common.returns import gae_targets,normalize_advantages

def example():
    f=lambda x:torch.tensor(x,dtype=torch.float64)
    b=lambda x:torch.tensor(x,dtype=torch.bool)
    return [f([1,2,3]),f([10,20,30]),f([20,7,40]),f([.9,.9,.9]),b([0,0,1]),b([0,1,1]),.8]

def test_actual_endpoint_and_separate_episode_trace():
    args=example();a,r=gae_targets(*args)
    torch.testing.assert_close(a,torch.tensor([.576,-11.7,-27],dtype=torch.float64))
    torch.testing.assert_close(r,torch.tensor([10.576,8.3,3],dtype=torch.float64))
    args[0][-1]=999
    _,changed=gae_targets(*args);torch.testing.assert_close(changed[:2],r[:2])
    args[4][1]=True
    _,terminated=gae_targets(*args);assert terminated[1]==2

def test_rollout_end_bootstrap_and_lambda_extremes():
    x=lambda v:torch.tensor(v,dtype=torch.float32)
    no=torch.tensor([False,False])
    args=[x([1,2]),x([3,4]),x([4,5]),x([.5,.25]),no,no]
    _,r=gae_targets(*args,1.)
    torch.testing.assert_close(r,x([2.625,3.25]))
    a,_=gae_targets(*args,0.)
    torch.testing.assert_close(a,x([0,-.75]))

def test_detached_owned_targets_and_singleton_normalization():
    args=example();args[1].requires_grad_();saved=[x.clone() for x in args[:-1]]
    a,r=gae_targets(*args);assert not a.requires_grad and not r.requires_grad
    for x,y in zip(args[:-1],saved):torch.testing.assert_close(x,y)
    one=torch.tensor([-2.]);out=normalize_advantages(one)
    torch.testing.assert_close(out,one);assert out.data_ptr()!=one.data_ptr()
    torch.testing.assert_close(normalize_advantages(torch.tensor([5.,5.])),torch.zeros(2))

@pytest.mark.parametrize('index,bad',[(0,torch.tensor([])),(0,torch.tensor([float('nan')]*3)),(3,torch.tensor([1.1]*3,dtype=torch.float64)),(4,torch.zeros(3)),(2,torch.zeros(4,dtype=torch.float64)),(6,True),(6,-.1)])
def test_bad_contract_rejected(index,bad):
    args=example();args[index]=bad
    with pytest.raises((ValueError,TypeError)):gae_targets(*args)

def test_inconsistent_boundary_and_overflow():
    args=example();args[5][-1]=False
    with pytest.raises(ValueError):gae_targets(*args)
    args=example();args[0].fill_(1.7e308);args[1].fill_(-1.7e308)
    with pytest.raises(ValueError,match='overflow'):gae_targets(*args)

@pytest.mark.parametrize('ratio,adv,expected,flat',[(1.5,1.,-1.2,True),(.5,-1.,.8,True),(1.5,-1.,1.5,False),(.5,1.,-.5,False)])
def test_clipping_signs_and_fixed_old_targets(ratio,adv,expected,flat):
    from common.ppo_loss import clipped_policy_loss
    new=torch.tensor([ratio],dtype=torch.float64).log().requires_grad_()
    old=torch.zeros(1,dtype=torch.float64,requires_grad=True)
    advantage=torch.tensor([adv],dtype=torch.float64,requires_grad=True)
    loss,kl,fraction=clipped_policy_loss(new,old,advantage,.2)
    assert float(loss.detach())==pytest.approx(expected)
    assert kl>=0 and fraction==1
    loss.backward()
    assert (new.grad.item()==0)==flat
    assert old.grad is None and advantage.grad is None

def test_ratio_overflow_is_rejected():
    from common.ppo_loss import clipped_policy_loss
    with pytest.raises(ValueError,match='overflow'):
        clipped_policy_loss(torch.tensor([1000.]),torch.zeros(1),torch.ones(1),.2)
