"""Actual Session combat and joint-transition boundaries, without training."""
import argparse
from pathlib import Path
import sys
import unittest
import numpy as np
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
args=p.parse_args();sys.path.insert(0,args.module_dir)
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
from gymnasium.utils.env_checker import check_env,data_equivalence
from gymnasium.error import ResetNeeded
from versus_env import VersusEnv
from versus_reward import versus_reward,finite_coefficient
from reward_runner import board_potential
import study_py as sim


def greedy(s):
 best=None
 for action in s.legal_actions():
  c=s.clone();e=c.apply_action(action);g=c.grid();rows=len(g);cols=len(g[0])
  heights=[];holes=0
  for col in range(cols):
   first=next((r for r in range(rows) if g[r][col]),rows)
   heights.append(rows-first);holes+=sum(not g[r][col] for r in range(first,rows))
  value=10*e['lines']-sum(heights)-4*holes-.5*sum(abs(a-b) for a,b in zip(heights,heights[1:]))
  option=(value,-action,action)
  if best is None or option>best:best=option
 return None if best is None else best[2]

class First:
    def reseed(self,seed):pass
    def reset(self):pass
    def act(self,obs,mask):
        actions=np.flatnonzero(mask)
        return int(actions[0]) if actions.size else None


class VersusContract(unittest.TestCase):
    def test_checker_and_random_opponent_replay(self):
        check_env(VersusEnv(),skip_render_check=True)
        env=VersusEnv(max_steps=20)
        def trace():
            _,info=env.reset(seed=11);out=[]
            for _ in range(20):
                action=greedy(env._session)
                result=env.step(action if action is not None else 0)
                out.append(result)
                if result[2] or result[3]:break
            return out
        self.assertTrue(data_equivalence(trace(),trace(),exact=True))

    def test_garbage_boundary_and_copy(self):
        s=sim.Session(11);before=s.state_bytes()
        with self.assertRaises(ValueError):s.add_garbage(-1)
        self.assertEqual(before,s.state_bytes())
        clone=s.clone();clone.add_garbage(2)
        self.assertEqual(s.pending_garbage(),0)
        self.assertEqual(clone.pending_garbage(),2)
        self.assertEqual(s.grid(),clone.grid())
        clone.apply_action(clone.legal_actions()[0])
        self.assertEqual(clone.last_garbage(),2)
        self.assertEqual(clone.pending_garbage(),0)

    def test_reward_boundaries(self):
        self.assertEqual(versus_reward(2,3,False,True,.5,10,7),13.5)
        self.assertEqual(versus_reward(2,3,True,True,.5,10,7),-3.5)
        for value in [True,'1',float('nan'),float('inf'),10**1000]:
            with self.assertRaises(ValueError):finite_coefficient(value,'coefficient')
        with self.assertRaises(ValueError):versus_reward(0,2,False,False,1.7e308,0,0)

    def test_callback_failure_preserves_both_boards(self):
        for bad in [True,1.5,'1',-1,10**6,None]:
            class Bad(First):
                def act(self,obs,mask):
                    obs['board'][:]=1;mask[:]=True
                    return bad
            env=VersusEnv(opponent=Bad());_,info=env.reset(seed=11)
            before=(env._session.state_bytes(),env._other.state_bytes(),env._steps)
            with self.assertRaises(ValueError):env.step(int(np.flatnonzero(info['legal_mask'])[0]))
            self.assertEqual(before,(env._session.state_bytes(),env._other.state_bytes(),env._steps))
            with self.assertRaises(ResetNeeded):env.step(0)

    def test_callback_uses_opponent_observation_and_copies(self):
        class Capture(First):
            def act(self,obs,mask):
                self.saved={k:v.copy() for k,v in obs.items()}
                action=super().act(obs,mask)
                obs['board'][:]=1;mask[:]=False
                return action
        policy=Capture();env=VersusEnv(opponent=policy);_,info=env.reset(seed=11)
        from observation import observe
        expected=observe(env._other,env._observation_schema)
        env.step(int(np.flatnonzero(info['legal_mask'])[0]))
        self.assertTrue(data_equivalence(expected,policy.saved,exact=True))
        self.assertFalse(np.all(np.asarray(env._other.grid())==1))

    def test_native_reference_routing(self):
        # Use an actual candidate evaluator only as a fixture; no model training.
        env=None
        class GreedyOther(First):
            def act(self,obs,mask):
                return greedy(env._other)
        attacks=0
        env=VersusEnv(opponent=GreedyOther(),max_steps=120)
        _,info=env.reset(seed=1)
        for _ in range(120):
            action=greedy(env._session)
            if action is None:action=0
            a,b=env._session.clone(),env._other.clone()
            before_a,before_b=a.attack_sent(),b.attack_sent()
            applied=action in a.legal_actions()
            attack_a=attack_b=0
            if applied:
                a.apply_action(action);attack_a=a.attack_sent()-before_a
                if attack_a and not b.finished():b.add_garbage(attack_a)
                if not b.finished():
                    choice=greedy(env._other) # Same fixture policy before incoming queue.
                    if choice is not None:
                        b.apply_action(choice);attack_b=b.attack_sent()-before_b
                        if attack_b and not a.finished():a.add_garbage(attack_b)
            obs,reward,term,trunc,info=env.step(action)
            self.assertEqual(a.state_bytes(),env._session.state_bytes())
            self.assertEqual(b.state_bytes(),env._other.state_bytes())
            self.assertEqual((attack_a,attack_b),(info['agent_attack'],info['opp_attack']))
            if attack_a and applied:
                self.assertEqual(env._other.last_garbage(),min(attack_a,sim.ROWS))
            if not env._session.finished() and applied:
                self.assertEqual(env._session.pending_garbage(),min(attack_b,sim.ROWS))
            attacks+=attack_a+attack_b
            if term or trunc:break
        self.assertGreater(attacks,0,'fixture must route actual attacks')

    def test_blocked_budget_and_terminal_recall(self):
        env=VersusEnv(max_steps=1);_,info=env.reset(seed=11)
        action=int(np.flatnonzero(~info['legal_mask'])[0])
        before=env._other.state_bytes()
        _,reward,term,trunc,info=env.step(action)
        self.assertEqual(before,env._other.state_bytes())
        self.assertEqual(reward,0);self.assertTrue(trunc);self.assertFalse(term)
        with self.assertRaises(ResetNeeded):env.step(action)
        env=VersusEnv(opponent=First());_,info=env.reset(seed=11)
        env._other.add_garbage(sim.ROWS)
        _,_,term,_,_=env.step(int(np.flatnonzero(info['legal_mask'])[0]))
        self.assertTrue(term)
        with self.assertRaises(ResetNeeded):env.step(0)

if __name__=='__main__':unittest.main(argv=[__file__])
