"""Input contracts and deterministic choice for the hand-crafted baseline."""
import numpy as np
import pytest
from common import BOARD_ROWS,BOARD_COLS
from common.features import all_features,bcts_score

@pytest.mark.parametrize('board',[
 np.zeros((BOARD_ROWS-1,BOARD_COLS)),np.zeros((BOARD_COLS,BOARD_ROWS)),
 np.full((BOARD_ROWS,BOARD_COLS),np.nan),np.full((BOARD_ROWS,BOARD_COLS),np.inf),
 np.full((BOARD_ROWS,BOARD_COLS),.25),np.full((BOARD_ROWS,BOARD_COLS),-1),
 np.zeros((BOARD_ROWS,BOARD_COLS),dtype=complex)])
def test_invalid_board_rejected(board):
 with pytest.raises(ValueError):all_features(board,0)

@pytest.mark.parametrize('lines',[True,1.5,'1',-1])
def test_invalid_line_count_rejected(lines):
 with pytest.raises(ValueError):all_features(np.zeros((BOARD_ROWS,BOARD_COLS)),lines)

@pytest.mark.parametrize('coefficient',[float('nan'),float('inf'),'1',True])
def test_invalid_greedy_coefficient_rejected(coefficient):
 from common.env_versus import GreedyBCTSOpponent
 with pytest.raises(ValueError):GreedyBCTSOpponent(line_weight=coefficient)


def test_raw_ghost_marker_matches_occupancy_contract():
 board=np.zeros((BOARD_ROWS,BOARD_COLS));board[-2,0]=8;board[-1,0]=9
 f=all_features(board,0)
 assert f['aggregate_height']==1 and f['holes']==0


def test_known_geometry_and_distinct_maximum():
 from common.features import column_heights,well_sum
 board=np.zeros((BOARD_ROWS,BOARD_COLS),dtype=int)
 board[-4,0]=1;board[-1,0]=1;board[-2:,1]=1
 f=all_features(board,2)
 assert f['aggregate_height']==6 and f['holes']==2 and f['max_height']==4
 assert f['bumpiness']==4 and f['rows_cleared']==2
 high=np.full(BOARD_COLS,BOARD_ROWS,dtype=int);high[1]=BOARD_ROWS-3
 assert well_sum(high)==6
 a=np.zeros(BOARD_COLS,dtype=int);b=a.copy();a[0]=4;b[:2]=2
 assert a.sum()==b.sum() and a.max()!=b.max()


def test_python_and_cpp_choices_follow_their_own_formulas():
 import os,json,subprocess
 from pathlib import Path
 from common.env_versus import GreedyBCTSOpponent
 from common.action_mask import encode_action
 from common.features import BCTS_WEIGHTS
 folder=os.environ.get('TETRIS_PY_MODULE_DIR');driver=os.environ.get('TETRIS_HEURISTIC_DUMP')
 if not folder or not driver:pytest.skip('select fresh native module and heuristic_dump')
 import tetris_py
 assert Path(tetris_py.__file__).resolve().parent==Path(folder).resolve()
 differences=0;positive=0
 for seed in [1,11,23]:
  rows=[json.loads(x) for x in subprocess.check_output([driver,str(seed),'90'],text=True).splitlines()]
  sim=tetris_py.SimGame(seed)
  for row in rows:
   before=sim.state_hash();assert row['hash']==before
   expected_cpp=expected_python=None;cpp_best=python_best=-float('inf')
   for p in sim.legal_placements():
    child=sim.clone();lines=child.apply_placement(p.col,p.rot)
    f=all_features(np.asarray(child.grid()),lines)
    # Match the actual C++ arithmetic order, not Python's extended feature sum.
    score=(-.510066*f['aggregate_height']+.760666*lines
           -.356630*f['holes']-.184483*f['bumpiness'])
    action=encode_action(p.col,p.rot)
    if score>cpp_best:cpp_best,expected_cpp=score,action
    score=float(lines)+sum(BCTS_WEIGHTS[k]*v for k,v in f.items())
    if score>python_best:python_best,expected_python=score,action
   assert row['action']==(expected_cpp if expected_cpp is not None else -1)
   actual=GreedyBCTSOpponent().act(sim)
   assert actual==expected_python and sim.state_hash()==before
   differences+=actual!=expected_cpp
   if expected_cpp is None:break
   from common.action_mask import decode_action
   lines=sim.apply_placement(*decode_action(expected_cpp));positive+=lines>0
   assert row['lines']==lines and row['after']==sim.state_hash()
 assert differences>0 and positive>0


def test_unsigned_heights_do_not_wrap_differences():
 from common.features import bumpiness
 heights=np.zeros(BOARD_COLS,dtype=np.uint8);heights[0]=4
 assert bumpiness(heights)==4


def test_changed_weight_cannot_emit_nan(monkeypatch):
 from common.features import BCTS_WEIGHTS
 monkeypatch.setitem(BCTS_WEIGHTS,'holes',float('nan'))
 with pytest.raises(ValueError,match='finite'):
  bcts_score(np.zeros((BOARD_ROWS,BOARD_COLS)),0)
