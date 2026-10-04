import argparse
import sys
import numpy as np
from actions import decode,legal_mask

parser=argparse.ArgumentParser();parser.add_argument('--module-dir',required=True)
args=parser.parse_args();sys.path.insert(0,args.module_dir)
import study_py as sim

game=sim.Session(77)
schema=sim.action_schema()
mask=legal_mask(game,schema)
available=np.flatnonzero(mask)
if not len(available):raise RuntimeError('no route in this action family')
action=int(available[0])
trace=game.action_trace(action)
replay=game.clone()
result=game.apply_action(action)
for frame in trace:replay.step(frame)
print('target (column, quarter):',decode(action,schema))
print('consumed input ticks:',result['ticks'])
print('same state as tick replay:',game.state_bytes()==replay.state_bytes())
saved=game.state_bytes()
try:game.apply_action(-1)
except ValueError:print('rejected action preserves state:',game.state_bytes()==saved)
