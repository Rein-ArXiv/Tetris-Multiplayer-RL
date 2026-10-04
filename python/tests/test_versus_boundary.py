"""Opponent callbacks cannot corrupt committed boards or fabricate a valid action."""
import os
from pathlib import Path
import sys
import numpy as np
import pytest
pytest.importorskip('gymnasium')
from gymnasium.error import ResetNeeded

@pytest.fixture(scope='module')
def api():
    folder=os.environ.get('TETRIS_PY_MODULE_DIR')
    if not folder:pytest.skip('select fresh native module')
    import tetris_py
    assert Path(tetris_py.__file__).resolve().parent==Path(folder).resolve()
    from common.env_versus import TetrisVersusEnv,VersusOpponent,PolicyOpponent
    return TetrisVersusEnv,VersusOpponent,PolicyOpponent

@pytest.mark.parametrize('result',[1.5,True,'0',-1,10**6,None])
def test_bad_opponent_result_preserves_boards_and_requires_reset(api,result):
    Env,Opponent,_=api
    class Bad(Opponent):
        def act(self,sim):return result
    env=Env(seed=11,opponent=Bad());_,info=env.reset()
    before=env.simA.state_hash(),env.simB.state_hash(),env._pieces
    with pytest.raises(ValueError):env.step(int(np.flatnonzero(info['legal_mask'])[0]))
    assert before==(env.simA.state_hash(),env.simB.state_hash(),env._pieces)
    with pytest.raises(ResetNeeded):env.step(0)


def test_callback_exception_and_mutation_are_isolated(api):
    Env,Opponent,_=api
    class Broken(Opponent):
        def act(self,sim):
            sim.add_pending_garbage(3)
            raise RuntimeError('policy failure')
    env=Env(seed=11,opponent=Broken());_,info=env.reset()
    before=env.simA.state_hash(),env.simB.state_hash()
    with pytest.raises(RuntimeError,match='policy failure'):
        env.step(int(np.flatnonzero(info['legal_mask'])[0]))
    assert before==(env.simA.state_hash(),env.simB.state_hash())
    with pytest.raises(ResetNeeded):env.step(0)


def test_callback_success_mutation_is_discarded(api):
    Env,Opponent,_=api
    class Mutator(Opponent):
        def act(self,sim):
            action=sim.legal_placements()[0]
            sim.add_pending_garbage(3)
            from common.action_mask import encode_action
            return encode_action(action.col,action.rot)
    env=Env(seed=11,opponent=Mutator());_,info=env.reset()
    env.step(int(np.flatnonzero(info['legal_mask'])[0]))
    assert env.simB.last_garbage_received()==0 and env.simB.pending_garbage()==0

@pytest.mark.parametrize('name',['attack_weight','win_bonus','loss_penalty'])
@pytest.mark.parametrize('value',[float('nan'),float('inf'),'1',True])
def test_reward_coefficients_are_finite_numbers(api,name,value):
    with pytest.raises(ValueError):api[0](**{name:value})


def test_policy_adapter_does_not_coerce_float(api):
    Env,_,Policy=api
    env=Env(seed=11,opponent=Policy(lambda obs,mask:0.5));_,info=env.reset()
    with pytest.raises(ValueError):env.step(int(np.flatnonzero(info['legal_mask'])[0]))


def test_blocked_opponent_action_is_rejected(api):
    Env,Opponent,_=api
    class Blocked(Opponent):
        def act(self,sim):
            from common.action_mask import legal_mask
            return int(np.flatnonzero(~legal_mask(sim).numpy())[0])
    env=Env(seed=11,opponent=Blocked());_,info=env.reset()
    before=env.simA.state_hash(),env.simB.state_hash()
    with pytest.raises(ValueError,match='blocked'):env.step(int(np.flatnonzero(info['legal_mask'])[0]))
    assert before==(env.simA.state_hash(),env.simB.state_hash())


def test_reward_overflow_does_not_commit_actual_attack(api):
    from common.env_versus import GreedyBCTSOpponent
    env=api[0](seed=11,opponent=GreedyBCTSOpponent(),attack_weight=1.7e308,win_bonus=1.7e308,max_pieces=200)
    env.reset();player=GreedyBCTSOpponent()
    for _ in range(200):
        action=player.act(env.simA)
        assert action is not None
        from common.action_mask import decode_action
        from common import BOARD_ROWS
        candidate=env.simA.clone();candidate.apply_placement(*decode_action(action))
        if candidate.attack_lines_sent()>env.simA.attack_lines_sent():
            env.simB.add_pending_garbage(BOARD_ROWS)
        before=env.simA.state_hash(),env.simB.state_hash(),env._pieces
        try:_,_,term,trunc,_=env.step(action)
        except ValueError as exc:
            assert 'finite' in str(exc)
            assert before==(env.simA.state_hash(),env.simB.state_hash(),env._pieces)
            with pytest.raises(ResetNeeded):env.step(action)
            return
        if term or trunc:break
    pytest.fail('fixture must reach an overflowing actual attack reward')


def test_reset_hook_failure_blocks_step(api):
    Env,Opponent,_=api
    class FailingReset(Opponent):
        fails=False
        def reset(self):
            if self.fails:raise RuntimeError('reset failure')
        def act(self,sim):return 0
    opponent=FailingReset();env=Env(seed=11,opponent=opponent);env.reset()
    opponent.fails=True
    with pytest.raises(RuntimeError,match='reset failure'):env.reset()
    with pytest.raises(ResetNeeded):env.step(0)
