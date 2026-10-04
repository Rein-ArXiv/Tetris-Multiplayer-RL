import argparse
import sys
import numpy as np
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
from gym_env import RoundEnv
from rewards import RewardSpec
from gymnasium.error import ResetNeeded

env=RoundEnv(max_steps=6,reward_spec=RewardSpec(shaping_scale=0))
obs,info=env.reset(seed=77)
print('episode seed:',info['episode_seed'],'observation contract:',env.observation_space.contains(obs))
while True:
    action=int(np.flatnonzero(info['legal_mask'])[0])
    obs,reward,terminated,truncated,info=env.step(action)
    print('decision/ticks/reward/end/cutoff:',info['decisions'],info['ticks'],reward,terminated,truncated)
    if terminated or truncated:break
try:env.step(action)
except ResetNeeded:print('reset required after episode boundary')
_,next_info=env.reset();print('next episode seed:',next_info['episode_seed'])
env.close();env.close()
