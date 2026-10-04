"""Keep training reward, game score and environment events distinct."""
import importlib
import os
from pathlib import Path
import sys
import numpy as np
import pytest

pytest.importorskip("torch")
from common import BOARD_ROWS,BOARD_COLS
from train.ppo_tetris import board_features,shaping_reward
from train.rl_common import bcts_shaped_reward
from common.features import bcts_score


def test_dense_shaping_validates_schema_and_numbers():
    for board in [np.zeros((BOARD_COLS,BOARD_ROWS)),
                  np.full((1,BOARD_ROWS,BOARD_COLS),np.nan),
                  np.full((1,BOARD_ROWS,BOARD_COLS),0.25)]:
        with pytest.raises(ValueError):board_features(board)
    for coefficient in [float('nan'),float('inf')]:
        with pytest.raises(ValueError):shaping_reward(np.zeros((1,BOARD_ROWS,BOARD_COLS)),coefficient)


def test_dense_penalty_and_bcts_are_different_objectives():
    board=np.zeros((1,BOARD_ROWS,BOARD_COLS),dtype=np.float32)
    board[0,-2,0]=1  # Height two with one hole beneath.
    holes,height,bump=board_features(board)
    assert (holes,height,bump)==(1,2,2)
    dense=shaping_reward(board,1)
    assert dense<0 and shaping_reward(board,0)==0
    obs={'board':board}
    assert bcts_shaped_reward(obs,2,0)==2
    assert bcts_shaped_reward(obs,2,0.5)==pytest.approx(2+0.5*bcts_score(board[0],2))
    # The same static penalty is charged again on each visit, unlike a difference.
    assert sum([dense]*3)==pytest.approx(3*dense)


@pytest.fixture(scope='module')
def environments():
    pytest.importorskip('gymnasium')
    folder=os.environ.get('TETRIS_PY_MODULE_DIR')
    if not folder:pytest.skip('select a freshly built tetris_py module')
    sys.path.insert(0,folder)
    native=importlib.import_module('tetris_py')
    assert Path(native.__file__).resolve().parent==Path(folder).resolve()
    from common.env import TetrisPlacementEnv
    from common.env_versus import TetrisVersusEnv,GreedyBCTSOpponent
    return TetrisPlacementEnv,TetrisVersusEnv,GreedyBCTSOpponent


def test_single_reward_is_event_lines_not_score_delta(environments):
    Single,_,Greedy=environments
    env=Single(seed=23);env.reset();policy=Greedy();positive=0;different=0
    for _ in range(120):
        action=policy.act(env.sim)
        if action is None:break
        from common.action_mask import decode_action
        col,rot=decode_action(action)
        clone=env.sim.clone();lines=clone.apply_placement(col,rot)
        score=env.sim.score()
        _,reward,term,trunc,info=env.step(action)
        assert reward==float(lines)
        assert env.sim.state_hash()==clone.state_hash()
        positive+=reward>0;different+=reward!=info['score']-score
        if term or trunc:break
    assert positive>0 and different>0


def test_versus_reward_terms_and_illegal_noop(environments):
    _,Versus,Greedy=environments
    from common.env_versus import _terminal_bonus
    env=Versus(seed=11,opponent=Greedy(),max_pieces=150)
    _,info=env.reset();player=Greedy();attacks=0
    before=(env.simA.state_hash(),env.simB.state_hash(),env._pieces)
    with pytest.raises(ValueError):env.step(10**6)
    assert before==(env.simA.state_hash(),env.simB.state_hash(),env._pieces)
    blocked=int(np.flatnonzero(~info['legal_mask'])[0])
    _,reward,term,trunc,info=env.step(blocked)
    assert reward==0 and not term and not trunc and not info['action_applied']
    assert before[:2]==(env.simA.state_hash(),env.simB.state_hash())
    assert env._pieces==before[2]+1
    for _ in range(150):
        action=player.act(env.simA)
        if action is None:break
        from common.action_mask import decode_action
        clone=env.simA.clone();lines=clone.apply_placement(*decode_action(action))
        _,reward,term,trunc,info=env.step(action)
        expected=lines+env.attack_weight*info['agent_attack']
        if term:expected+=_terminal_bonus(env.simA.game_over(),env.simB.game_over(),env.win_bonus,env.loss_penalty)
        assert reward==pytest.approx(expected)
        attacks+=info['agent_attack']
        if term or trunc:break
    assert attacks>0
    assert _terminal_bonus(True,True,10,7)==-7
    assert _terminal_bonus(False,True,10,7)==10
    assert _terminal_bonus(True,False,10,7)==-7
    assert _terminal_bonus(False,False,10,7)==0
