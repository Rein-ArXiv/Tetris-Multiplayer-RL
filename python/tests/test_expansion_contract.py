import numpy as np
import pytest
from netbot.input_expander import expand_placement,INPUT_RIGHT,INPUT_DROP
from common import NUM_COLS,NUM_ROTATIONS

@pytest.mark.parametrize('index',range(5))
@pytest.mark.parametrize('bad',[True,False,1.5,'1',None,np.bool_(True)])
def test_no_implicit_narrowing(index,bad):
    args=[3,0,0,0,NUM_ROTATIONS];args[index]=bad
    with pytest.raises(TypeError):expand_placement(*args)

@pytest.mark.parametrize('args',[[-NUM_COLS-1,0,0,0],[NUM_COLS,0,0,0],[0,0,-1,0],[0,0,NUM_COLS,0],
    [0,-1,0,0],[0,0,0,NUM_ROTATIONS],[0,0,0,0,NUM_ROTATIONS+1],[2**100,0,0,0],[0,0,0,2**100]])
def test_bound_before_allocating(args):
    with pytest.raises(ValueError):expand_placement(*args)

def test_negative_origin_and_numpy_integral():
    assert expand_placement(np.int64(-1),np.int64(0),np.int64(0),np.int64(0))==[INPUT_RIGHT,INPUT_DROP]
