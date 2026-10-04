import argparse
from pathlib import Path
import sys
import unittest
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
import study_py as sim
from gymnasium.utils.env_checker import check_env
from heuristic_features import extract
from heuristic import Weights,GreedyPolicy,evaluate,PlanningVersusEnv

class HeuristicContract(unittest.TestCase):
 def test_features_and_invalid_values(self):
  f=extract([[1,0,0],[0,1,0],[0,1,0],[1,1,0]])
  self.assertEqual((f.heights,f.holes,f.aggregate_height,f.bumpiness,f.max_height,f.wells),
                   ((4,3,0),2,7,4,4,6))
  self.assertEqual(extract([[0,0],[0,0]]).aggregate_height,0)
  for board in [[],[[]],[[1],[0,1]],[[.5]],[[1.]],[[float('nan')]],'01',[[2]]]:
   with self.assertRaises(ValueError):extract(board)
  for value in [float('nan'),float('inf'),True,'1']:
   with self.assertRaises(ValueError):Weights(holes=value)
  with self.assertRaises(ValueError):evaluate(f,0,Weights(aggregate_height=1.7e308))

 def test_actual_choice_preserves_and_matches_candidates(self):
  s=sim.Session(1);policy=GreedyPolicy();clears=0
  for _ in range(60):
   before=s.state_bytes();decision=policy.choose(s)
   self.assertEqual(before,s.state_bytes())
   options=[]
   for action in s.legal_actions():
    c=s.clone();event=c.apply_action(action);f=extract(c.grid())
    options.append((not c.finished(),evaluate(f,event['lines'],policy.weights),-action))
   if not options:
    self.assertIsNone(decision);break
   self.assertEqual(decision.action,-max(options)[2])
   self.assertEqual(decision.candidates,len(options))
   event=s.apply_action(decision.action);clears+=event['lines']
   if s.finished():break
  self.assertGreater(clears,0)

 def test_exact_ties_and_survival_priority(self):
  zeros=Weights(0,0,0,0,0,0)
  no_priority=GreedyPolicy(zeros,prefer_survival=False);s=sim.Session(11)
  self.assertEqual(no_priority.choose(s).action,min(s.legal_actions()))
  # Find a real board where ending and surviving actions coexist.
  for _ in range(200):
   actions=s.legal_actions()
   alive=[];ended=[]
   for action in actions:
    c=s.clone();c.apply_action(action)
    (ended if c.finished() else alive).append(action)
   if alive and ended:
    self.assertIn(GreedyPolicy(zeros).choose(s).action,alive)
    self.assertEqual(no_priority.choose(s).action,min(actions))
    return
   if not actions:break
   s.apply_action(actions[0])
  self.fail('must exercise mixed actual terminal candidates')

 def test_planning_adapter_and_replay(self):
  check_env(PlanningVersusEnv(),skip_render_check=True)
  env=PlanningVersusEnv(max_steps=20);policy=GreedyPolicy()
  def trace():
   env.reset(seed=1);out=[]
   for _ in range(20):
    decision=policy.choose(env._session)
    _,r,t,c,info=env.step(decision.action if decision else 0)
    out.append((info['state_bytes'],info['opponent_state'],r))
    if t or c:break
   return out
  self.assertEqual(trace(),trace())

if __name__=='__main__':unittest.main(argv=[__file__])
