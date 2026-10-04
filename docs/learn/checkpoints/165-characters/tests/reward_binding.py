import argparse
from pathlib import Path
import sys
import math
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
from rewards import RewardSpec
from reward_runner import execute

p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
import study_py as sim

ended_count=0
for seed in range(1,9):
    session=sim.Session(seed)
    discounted_shaping=0.;multiplier=1.;initial=None
    spec=RewardSpec(gamma=0.9,shaping_scale=0.3,time_basis='tick',terminal_penalty=2)
    for _ in range(100):
        legal=session.legal_actions()
        if not legal:break
        original=session.state_bytes();trace=session.action_trace(legal[0])
        candidate,t=execute(session,legal[0],spec)
        assert session.state_bytes()==original
        replay=session.clone()
        for frame in trace:replay.step(frame)
        assert replay.state_bytes()==candidate.state_bytes()
        assert t.ticks==len(trace) and t.score_delta==candidate.score()-session.score()
        assert math.isfinite(t.reward.total)
        if initial is None:initial=t.before_potential
        discounted_shaping+=multiplier*t.reward.shaping
        multiplier*=t.reward.discount
        endpoint=0. if t.terminated else t.after_potential
        assert math.isclose(discounted_shaping,spec.shaping_scale*(-initial+multiplier*endpoint),abs_tol=1e-10)
        session=candidate
        if t.terminated:ended_count+=1;break
    before=session.state_bytes()
    try:execute(session,-1,spec)
    except ValueError:pass
    else:raise AssertionError('invalid action accepted')
    assert session.state_bytes()==before
assert ended_count>0
print('Actual Round reward components, tick replay, telescoping and terminal paths passed')

# Seek a real clear rather than accepting an all-zero base-reward rollout.
session=sim.Session(23)
clears=0;score_differs=False
for _ in range(160):
    candidates=[execute(session,action,RewardSpec(shaping_scale=0))
                for action in session.legal_actions()]
    if not candidates:break
    candidate,t=max(candidates,key=lambda item:(item[1].lines,item[1].after_potential))
    assert t.reward.base==t.lines and t.reward.total==t.lines
    clears+=t.lines
    score_differs|=t.lines!=t.score_delta
    session=candidate
    if clears>0 and score_differs:break
    if t.terminated:break
assert clears>0 and score_differs,'exercise real clear and distinct game points'

# A reward overflow after a successful candidate action must preserve the input.
session=sim.Session(77);rejected_reward=False
for _ in range(30):
    legal=session.legal_actions()
    if not legal:break
    before=session.state_bytes()
    for action in legal:
        try:execute(session,action,RewardSpec(shaping_scale=1.7e308))
        except ValueError:
            assert session.state_bytes()==before
            rejected_reward=True;break
    if rejected_reward:break
    session.apply_action(legal[0])
assert rejected_reward,'exercise arithmetic rejection after an actual transition'
print('Nonzero line reward, score distinction and failed reward commit passed')
