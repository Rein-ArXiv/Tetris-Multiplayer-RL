import argparse
import sys
from rewards import RewardSpec
from reward_runner import execute

parser=argparse.ArgumentParser();parser.add_argument('--module-dir',required=True)
args=parser.parse_args();sys.path.insert(0,args.module_dir)
import study_py as sim

session=sim.Session(77)
spec=RewardSpec(gamma=0.9,shaping_scale=0.25,time_basis='tick')
for step in range(6):
    legal=session.legal_actions()
    if not legal:break
    before=session.state_bytes()
    candidate,transition=execute(session,legal[step%len(legal)],spec)
    assert session.state_bytes()==before
    print('lines/score delta/ticks:',transition.lines,transition.score_delta,transition.ticks)
    print('base/shaping/terminal/total:',transition.reward)
    session=candidate
    if transition.terminated:break
print('Prepared rewards without mutating the input; committed candidates explicitly')
