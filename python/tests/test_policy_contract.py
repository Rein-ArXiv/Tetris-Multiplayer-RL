"""Policy axes, input metadata, gradients and graph compatibility."""
import pytest
torch = pytest.importorskip("torch")
from common import BOARD_ROWS as H, BOARD_COLS as W, NUM_PIECE_TYPES as K
from common.models import TetrisPolicyNet, masked_log_softmax, masked_entropy

torch.set_num_threads(1)

def sample(batch=2):
    board=torch.zeros(batch,1,H,W)
    board[:,0,H-1,0]=1
    current=torch.eye(K)[torch.arange(batch)%K]
    nxt=torch.eye(K)[(torch.arange(batch)+1)%K]
    return board,current,nxt

def model():
    torch.manual_seed(156)
    return TetrisPolicyNet(conv_channels=(2,),hidden=8)

@pytest.mark.parametrize('kwargs', [
    {'board_channels': True}, {'hidden':True}, {'hidden':0},
    {'n_placements':0}, {'n_piece_types':True}, {'conv_channels':()},
    {'conv_channels':(True,)}, {'conv_channels':(0,)}])
def test_reject_invalid_dimensions(kwargs):
    with pytest.raises((TypeError,ValueError)):
        TetrisPolicyNet(**kwargs)

def test_reject_same_area_transposed_board():
    b,c,n=sample()
    with pytest.raises(ValueError,match='board'):
        model()(b.transpose(-1,-2),c,n)

def test_reject_compensating_piece_widths():
    b,c,n=sample()
    # Total concat width unchanged; each semantic field is wrong.
    with pytest.raises(ValueError,match='current'):
        model()(b,c[:,:-1],torch.cat([n,n[:,:1]],-1))

def test_reject_empty_batch():
    with pytest.raises(ValueError,match='board'):
        model()(*sample(0))

@pytest.mark.parametrize('field', [0,1,2])
def test_reject_wrong_dtype_by_name(field):
    values=list(sample());values[field]=values[field].double()
    with pytest.raises(TypeError,match=('board','current','next')[field]):
        model()(*values)

def test_reject_wrong_device_before_arithmetic():
    b,c,n=sample()
    with pytest.raises(ValueError,match='current'):
        model()(b,c.to('meta'),n)

def test_reject_tensor_type_and_rank():
    b,c,n=sample()
    for bad in ([],b[0,0],b.unsqueeze(0)):
        with pytest.raises((TypeError,ValueError),match='board'):
            model()(bad,c,n)

def test_valid_outputs_alias_trace_and_gradients():
    net=model();batch=sample()
    logits,value=net(*batch)
    assert logits.shape==(2,net.n_placements) and value.shape==(2,)
    alias=net(batch[0].squeeze(1),*batch[1:])
    for a,b in zip((logits,value),alias):torch.testing.assert_close(a,b,rtol=0,atol=0)
    # Eager validation is not a runtime contract embedded in this graph.
    with torch.no_grad():
        traced=torch.jit.trace(net,batch)
        for a,b in zip(net(*batch),traced(*batch)):torch.testing.assert_close(a,b)
    mask=torch.ones_like(logits,dtype=torch.bool);mask[:,1::2]=False
    logp=masked_log_softmax(logits,mask)
    loss=-logp[:,0].mean()+(value-1).square().mean()-.01*masked_entropy(logp,mask).mean()
    old={k:v.detach().clone() for k,v in net.named_parameters()}
    opt=torch.optim.SGD(net.parameters(),lr=.01)
    opt.zero_grad(set_to_none=True);loss.backward()
    assert all(p.grad is not None and torch.isfinite(p.grad).all() for p in net.parameters())
    for prefix in ('trunk','policy_head','value_head'):
        assert any(p.grad.abs().sum()>0 for k,p in net.named_parameters() if k.startswith(prefix))
    assert all(torch.equal(p,old[k]) for k,p in net.named_parameters())
    opt.step();assert any(not torch.equal(p,old[k]) for k,p in net.named_parameters())

def test_eval_does_not_disable_grad_and_double_model_is_valid():
    net=model().double().eval()
    logits,value=net(*(x.double() for x in sample(1)))
    assert logits.dtype==value.dtype==torch.float64 and value.shape==(1,)
    assert value.requires_grad
    with torch.no_grad():assert not net(*(x.double() for x in sample(1)))[1].requires_grad
