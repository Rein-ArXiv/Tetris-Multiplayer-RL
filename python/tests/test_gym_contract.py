"""Actual Gymnasium protocol and native environment lifecycle regressions."""
import copy
import importlib
import os
from pathlib import Path
import sys
import numpy as np
import pytest
pytest.importorskip('gymnasium')
from gymnasium.utils.env_checker import check_env, data_equivalence
from gymnasium.error import ResetNeeded
from common import NUM_PLACEMENTS,BOARD_ROWS

@pytest.fixture(scope='module')
def classes():
    directory=os.environ.get('TETRIS_PY_MODULE_DIR')
    if not directory:pytest.skip('select fresh native module')
    folder=Path(directory).resolve();sys.path.insert(0,str(folder))
    native=importlib.import_module('tetris_py')
    assert Path(native.__file__).resolve().parent==folder
    from common.env import TetrisPlacementEnv
    from common.env_versus import TetrisVersusEnv,RandomLegalOpponent
    return TetrisPlacementEnv,TetrisVersusEnv,RandomLegalOpponent

@pytest.mark.parametrize('index',[0,1])
def test_official_checker(classes,index):
    env=classes[index]()
    check_env(env,skip_render_check=True)

@pytest.mark.parametrize('index',[0,1])
def test_seeded_reset_then_unseeded_stream(classes,index):
    env=classes[index](seed=71)
    def sequence():
        obs,info=env.reset(seed=71)
        values=[(info['episode_seed'],copy.deepcopy(obs))]
        for _ in range(3):
            obs,info=env.reset();values.append((info['episode_seed'],copy.deepcopy(obs)))
        return values
    first,second=sequence(),sequence()
    assert data_equivalence(first,second,exact=True)
    assert first[0][0]==71 and len(set(x[0] for x in first))>1
    ctor=classes[index](seed=71);_,info=ctor.reset();assert info['episode_seed']==71

@pytest.mark.parametrize('index',[0,1])
def test_lifecycle_and_unconverted_actions(classes,index):
    env=classes[index]()
    with pytest.raises(ResetNeeded):env.step(0)
    obs,info=env.reset(seed=73)
    native=env.sim if index==0 else env.simA
    before=native.state_hash()
    for action in [True,1.5,'1',-1,NUM_PLACEMENTS,2**200,np.array([0]),np.array(True)]:
        with pytest.raises(ValueError):env.step(action)
        assert native.state_hash()==before
    action=np.asarray(np.flatnonzero(info['legal_mask'])[0])
    env.step(action)
    env.close();env.close()
    with pytest.raises(ResetNeeded):env.step(0)
    env.reset(seed=73)


def test_random_opponent_reset_and_explicit_board_seed(classes):
    _,Versus,Random=classes
    env=Versus(opponent=Random(92),opponent_seed=501)
    def trace():
        _,info=env.reset(seed=99);states=[]
        for _ in range(12):
            action=int(np.flatnonzero(info['legal_mask'])[0])
            _,reward,term,trunc,info=env.step(action)
            states.append((env.simA.state_hash(),env.simB.state_hash(),reward))
            if term or trunc:break
        return states
    assert trace()==trace()
    assert env._opp_seed==501
    # The adjacent seed wraps in the native uint64 domain.
    default=Versus();default.reset(seed=(1<<64)-1);assert default._opp_seed==0


def test_truncation_counts_in_domain_noops_and_blocks_further_step(classes):
    _,Versus,_=classes
    env=Versus(max_pieces=1);_,info=env.reset(seed=11)
    illegal=int(np.flatnonzero(~info['legal_mask'])[0])
    before=env.simA.state_hash(),env.simB.state_hash()
    _,reward,term,trunc,after=env.step(illegal)
    assert (env.simA.state_hash(),env.simB.state_hash())==before
    assert reward==0 and not term and trunc and not after['action_applied']
    with pytest.raises(ResetNeeded):env.step(illegal)


def test_terminal_win_is_not_rewarded_again(classes):
    _,Versus,_=classes
    env=Versus(max_pieces=200);_,info=env.reset(seed=11)
    env.simB.add_pending_garbage(BOARD_ROWS)
    action=int(np.flatnonzero(info['legal_mask'])[0])
    _,_,term,_,_=env.step(action)
    assert term and env.simB.game_over()
    with pytest.raises(ResetNeeded):env.step(action)

@pytest.mark.parametrize('index',[0,1])
def test_invalid_reset_preserves_current_episode(classes,index):
    env=classes[index]();env.reset(seed=1)
    native=env.sim if index==0 else env.simA
    before=native.state_hash()
    for seed in [-1,True,1.5,1<<64]:
        with pytest.raises(ValueError):env.reset(seed=seed)
        assert native.state_hash()==before
    for options in [{'unknown':1},[],False,0,'']:
        with pytest.raises(ValueError):env.reset(options=options)
