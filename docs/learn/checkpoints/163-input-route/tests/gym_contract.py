import argparse
from copy import deepcopy
from pathlib import Path
import sys
import unittest
import numpy as np
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
from gymnasium.error import ResetNeeded
from gymnasium.utils.env_checker import check_env,data_equivalence
from gymnasium.wrappers import TimeLimit
from gym_env import RoundEnv
from gym_contract import optional_seed,action_index,positive_limit
from reward_runner import board_potential
from rewards import RewardSpec


class GymContract(unittest.TestCase):
    def test_official_checker(self):
        for seed in [0,11,77]:check_env(RoundEnv(seed=seed),skip_render_check=True)

    def test_seed_stream_and_separate_action_rng(self):
        env=RoundEnv(seed=71)
        def sequence():
            first=env.reset(seed=71)
            return [first]+[env.reset() for _ in range(3)]
        first,second=sequence(),sequence()
        self.assertTrue(data_equivalence(first,second,exact=True))
        self.assertEqual(first[0][1]['episode_seed'],71)
        self.assertGreater(len({item[1]['episode_seed'] for item in first}),1)
        self.assertEqual(RoundEnv(seed=71).reset()[1]['episode_seed'],71)
        env.action_space.seed(14)
        rng=deepcopy(env.action_space.np_random.bit_generator.state)
        env.reset(seed=14)
        self.assertEqual(env.action_space.np_random.bit_generator.state,rng)
        for seed in [0,(1<<64)-1]:
            self.assertEqual(env.reset(seed=seed)[1]['episode_seed'],seed)

    def test_boundary_validation_preserves_state(self):
        env=RoundEnv()
        with self.assertRaises(ResetNeeded):env.step(0)
        obs,info=env.reset(seed=77);before=info['state_bytes']
        for value in [True,np.bool_(True),1.5,'1',-1,env.action_space.n,np.array([0]),np.array(True)]:
            with self.assertRaises(ValueError):env.step(value)
            self.assertEqual(env._session.state_bytes(),before)
            self.assertEqual(env._steps,0)
        for seed in [True,-1,1.5,1<<64]:
            with self.assertRaises(ValueError):env.reset(seed=seed)
            self.assertEqual(env._session.state_bytes(),before)
        for options in [{'unknown':1},[],False,0,'']:
            with self.assertRaises(ValueError):env.reset(options=options)
        env.step(np.array(np.flatnonzero(info['legal_mask'])[0],dtype=np.int64))
        self.assertEqual(action_index(np.int64(1),2),1)
        self.assertEqual(optional_seed(np.uint64((1<<64)-1)),(1<<64)-1)
        for limit in [False,0,-1,1.5]:
            with self.assertRaises(ValueError):positive_limit(limit)

    def test_masked_noop_clock_and_budget(self):
        for clock in ['decision','tick']:
            spec=RewardSpec(gamma=0.9,shaping_scale=0.5,time_basis=clock)
            env=RoundEnv(max_steps=3,reward_spec=spec);_,info=env.reset(seed=77)
            for _ in range(2):_,_,_,_,info=env.step(int(np.flatnonzero(info['legal_mask'])[0]))
            before=env._session.state_bytes();phi=board_potential(env._session.grid())
            action=int(np.flatnonzero(~info['legal_mask'])[0])
            _,reward,term,trunc,info=env.step(action)
            self.assertEqual(before,env._session.state_bytes())
            self.assertEqual(info['decisions'],3);self.assertEqual(info['ticks'],0)
            self.assertFalse(info['action_applied']);self.assertFalse(term);self.assertTrue(trunc)
            discount=spec.gamma if clock=='decision' else 1.
            self.assertEqual(info['discount'],discount)
            self.assertAlmostEqual(reward,spec.shaping_scale*((discount-1)*phi))
            with self.assertRaises(ResetNeeded):env.step(action)

    def test_terminal_and_limit_can_coincide(self):
        def play(limit=None):
            env=RoundEnv(max_steps=limit);_,info=env.reset(seed=77)
            for _ in range(300):
                _,_,term,trunc,info=env.step(int(np.flatnonzero(info['legal_mask'])[0]))
                if term or trunc:return env,term,trunc,info
            self.fail('fixture must reach a boundary')
        env,term,trunc,info=play()
        self.assertTrue(term);self.assertFalse(trunc)
        count=info['decisions'];env,term,trunc,info=play(count)
        self.assertTrue(term and trunc)
        self.assertFalse(info['legal_mask'].any())
        with self.assertRaises(ResetNeeded):env.step(0)
        env.close();env.close()
        with self.assertRaises(ResetNeeded):env.step(0)
        self.assertTrue(env.observation_space.contains(env.reset(seed=77)[0]))

    def test_failed_reward_keeps_episode_and_counter(self):
        env=RoundEnv(reward_spec=RewardSpec(shaping_scale=1.7e308));_,info=env.reset(seed=77)
        for _ in range(30):
            before=env._session.state_bytes();steps=env._steps
            for action in np.flatnonzero(info['legal_mask']):
                try:result=env.step(int(action))
                except ValueError:
                    self.assertEqual(env._session.state_bytes(),before)
                    self.assertEqual(env._steps,steps)
                    self.assertFalse(env._needs_reset)
                    return
                else:
                    _,_,term,trunc,info=result
                    if term or trunc:self.fail('fixture ended before arithmetic rejection')
                    break
        self.fail('must exercise failed reward construction')

    def test_wrapper_cutoff_and_output_ownership(self):
        env=TimeLimit(RoundEnv(),max_episode_steps=2)
        first,info=env.reset(seed=77);saved={k:v.copy() for k,v in first.items()}
        for _ in range(2):obs,_,term,trunc,info=env.step(int(np.flatnonzero(info['legal_mask'])[0]))
        self.assertTrue(trunc);self.assertFalse(term)
        final=info['state_bytes']
        reset_obs,reset_info=env.reset(seed=77)
        self.assertNotEqual(final,reset_info['state_bytes'])
        self.assertTrue(data_equivalence(first,saved,exact=True))
        obs['board'][:]=1
        self.assertTrue(data_equivalence(first,reset_obs,exact=True))

if __name__=='__main__':unittest.main(argv=[__file__])
