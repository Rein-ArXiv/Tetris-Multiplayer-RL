import argparse
import sys
import numpy as np
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
from versus_env import VersusEnv

env=VersusEnv(max_steps=20)
obs,info=env.reset(seed=11)
while True:
    choices=np.flatnonzero(info['legal_mask'])
    action=int(choices[0]) if choices.size else 0
    obs,reward,term,trunc,info=env.step(action)
    print('decision/attacks/ticks/reward:',info['decisions'],info['agent_attack'],
          info['opp_attack'],info['ticks'],info['opponent_ticks'],reward)
    if term or trunc:break
print('two-board boundary:',term,trunc)
env.close()
