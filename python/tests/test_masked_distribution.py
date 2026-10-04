"""A finite forward entropy is insufficient: validate its actual gradients."""
import pytest
torch = pytest.importorskip("torch")
from common.models import masked_log_softmax
from train.ppo_tetris import _logp_entropy
from train.policy_gradient_tetris import _masked_logp_entropy


@pytest.mark.parametrize("entropy_fn", [lambda x,m: _logp_entropy(x,m)[2],
                                     lambda x,m: _masked_logp_entropy(x,m)[1]])
def test_actual_training_entropy_has_finite_correct_gradient(entropy_fn):
    logits = torch.tensor([[0., 1., 2.]], dtype=torch.float64, requires_grad=True)
    mask = torch.tensor([[True,False,True]])
    entropy = entropy_fn(logits,mask)
    entropy.sum().backward()
    assert torch.isfinite(entropy).all() and torch.isfinite(logits.grad).all()
    # Independent reference computes softmax over only the legal coordinates.
    legal = torch.tensor([0.,2.],dtype=torch.float64,requires_grad=True)
    lp = torch.log_softmax(legal,dim=-1)
    expected = -(lp.exp()*lp).sum()
    expected.backward()
    torch.testing.assert_close(entropy[0], expected.detach())
    torch.testing.assert_close(logits.grad[0,[0,2]],legal.grad)
    assert logits.grad[0,1] == 0


@pytest.mark.parametrize("mask",[torch.tensor([False,False]),
                                torch.tensor([[True,False],[False,False]])])
def test_no_legal_action_is_explicit_failure(mask):
    with pytest.raises(ValueError,match="legal"):
        masked_log_softmax(torch.zeros(mask.shape),mask)


def test_mask_shape_dtype_and_legal_finiteness():
    logits=torch.zeros((2,3))
    for mask in [torch.ones(3,dtype=torch.bool),torch.ones((2,3),dtype=torch.int64)]:
        with pytest.raises((TypeError,ValueError)):
            masked_log_softmax(logits,mask)
    for value in [float('nan'),float('inf'),float('-inf')]:
        with pytest.raises(ValueError,match="finite"):
            masked_log_softmax(torch.tensor([value,0.]),torch.tensor([True,False]))


def test_legal_probabilities_and_single_action():
    mask=torch.tensor([[True,False,True],[False,True,False]])
    logits=torch.tensor([[2.,float('nan'),1.],[1.,4.,5.]],requires_grad=True)
    logp=masked_log_softmax(logits,mask)
    assert torch.isneginf(logp[~mask]).all()
    torch.testing.assert_close(logp.exp().sum(-1),torch.ones(2))
    assert logp[1,1] == 0
    (-logp[0,0]).backward()
    assert torch.isfinite(logits.grad).all()

@pytest.mark.parametrize('helper',[_logp_entropy,_masked_logp_entropy])
def test_actual_network_update_stays_finite(helper):
    from common import BOARD_ROWS, BOARD_COLS, NUM_PIECE_TYPES, NUM_PLACEMENTS
    from common.models import TetrisPolicyNet
    torch.manual_seed(73)
    model=TetrisPolicyNet(conv_channels=(2,),hidden=8)
    optimizer=torch.optim.SGD(model.parameters(),lr=0.01)
    board=torch.zeros((1,1,BOARD_ROWS,BOARD_COLS))
    piece=torch.zeros((1,NUM_PIECE_TYPES));piece[0,0]=1
    logits,value=model(board,piece,piece)
    mask=torch.zeros((1,NUM_PLACEMENTS),dtype=torch.bool)
    mask[0,0]=mask[0,-1]=True
    result=helper(logits,mask)
    logp,entropy=result[0],result[-1]
    loss=-logp[0,0]-0.01*entropy.sum()+value.square().sum()
    loss.backward()
    assert all(p.grad is not None and torch.isfinite(p.grad).all() for p in model.parameters())
    before=model.policy_head.weight.detach().clone()
    optimizer.step()
    assert all(torch.isfinite(p).all() for p in model.parameters())
    assert not torch.equal(before,model.policy_head.weight)


def test_finite_logits_with_unrepresentable_log_difference_are_rejected():
    limit=torch.finfo(torch.float32).max
    with pytest.raises(ValueError,match="normalized"):
        masked_log_softmax(torch.tensor([limit,-limit]),torch.tensor([True,True]))
