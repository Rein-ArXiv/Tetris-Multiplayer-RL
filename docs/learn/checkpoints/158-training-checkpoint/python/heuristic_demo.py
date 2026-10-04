import argparse
import sys
import json
from dataclasses import asdict
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
from heuristic import GreedyPolicy,PlanningVersusEnv

policy=GreedyPolicy()
# These are fixed experiment seeds, not content counts or production defaults.
seeds=(1,11,23)
for seed in seeds:
    env=PlanningVersusEnv(max_steps=40);obs,info=env.reset(seed=seed)
    total_reward=total_lines=total_attack=candidates=0
    while True:
        decision=policy.choose(env._session)
        action=decision.action if decision is not None else 0
        candidates+=decision.candidates if decision else 0
        obs,reward,term,trunc,info=env.step(action)
        total_reward+=reward;total_lines+=info['lines_cleared'];total_attack+=info['agent_attack']
        if term or trunc:break
    print(json.dumps(dict(seed=seed,decisions=info['decisions'],lines=total_lines,
                         attack=total_attack,reward=total_reward,candidates=candidates,
                         terminated=term,truncated=trunc,weights=asdict(policy.weights))))
    env.close()
print('fixed-seed baseline complete')
