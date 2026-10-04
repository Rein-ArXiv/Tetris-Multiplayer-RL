import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'python'))
from rewards import RewardSpec,evaluate
from reward_runner import board_potential


class RewardContract(unittest.TestCase):
    def test_components_and_end_tick_timing(self):
        spec=RewardSpec(gamma=0.5,line_weight=2,shaping_scale=0.25,
                        terminal_penalty=3,time_basis='tick')
        p=evaluate(2,3,False,-2,-1,spec)
        self.assertEqual(p.discount,0.125)
        self.assertEqual(p.base,1.0)  # Four points on the third tick.
        self.assertEqual(p.shaping,0.46875)
        self.assertEqual(p.total,p.base+p.shaping+p.terminal)
        done=evaluate(2,3,True,-2,-1,spec)
        self.assertEqual(done.terminal,-0.75)
        self.assertEqual(done.shaping,0.5)
        decision=evaluate(2,3,False,-2,-1,RewardSpec(gamma=0.5,line_weight=2))
        self.assertEqual(decision.base,4)
        self.assertEqual(decision.discount,0.5)

    def test_potential_telescopes_with_variable_durations(self):
        potentials=[-3,-1,-4,20] # True terminal's stored board value is ignored.
        for clock in ['tick','decision']:
            spec=RewardSpec(gamma=0.8,shaping_scale=0.7,time_basis=clock)
            for terminal in [False,True]:
                cumulative=1.;shaping=0.
                for i,duration in enumerate([2,1,4]):
                    p=evaluate(0,duration,terminal and i==2,potentials[i],potentials[i+1],spec)
                    shaping+=cumulative*p.shaping
                    cumulative*=p.discount
                boundary=0 if terminal else potentials[-1]
                self.assertAlmostEqual(shaping,0.7*(-potentials[0]+cumulative*boundary))

    def test_disabled_shaping_avoids_irrelevant_overflow(self):
        p=evaluate(1,1,False,-1e308,1e308,RewardSpec(shaping_scale=0))
        self.assertEqual(p.shaping,0)
        self.assertEqual(p.total,1)

    def test_cutoff_keeps_endpoint_potential(self):
        spec=RewardSpec(gamma=0.9,shaping_scale=1)
        cutoff=evaluate(0,1,False,-4,-2,spec)
        terminal=evaluate(0,1,True,-4,-2,spec)
        self.assertAlmostEqual(cutoff.shaping,2.2)
        self.assertEqual(terminal.shaping,4)

    def test_dense_penalty_can_reverse_base_objective(self):
        # Sparse path A wins 1 at its end, but repeated -0.1 changes its ranking.
        base_a=[0.]*19+[1.];base_b=[0.5]
        self.assertGreater(sum(base_a),sum(base_b))
        self.assertLess(sum(r-0.1 for r in base_a),sum(base_b))
        # Potential differences with terminal Phi=0 add the same initial offset.
        for path in [base_a,base_b]:
            phis=[-2]+[-1]*(len(path)-1)+[0]
            shaped=sum(evaluate(0,1,i==len(path)-1,phis[i],phis[i+1],
                                RewardSpec(gamma=1,shaping_scale=1)).shaping
                       for i,r in enumerate(path))
            self.assertEqual(shaped,2)

    def test_board_potential_counts_internal_holes(self):
        board=[[0,1,0],[1,0,0],[0,1,0]]
        # heights 2,3,0 and one hole per occupied column.
        self.assertAlmostEqual(board_potential(board),-(5/3+2))
        self.assertEqual(board_potential([[0,0],[0,0]]),0)
        for bad in [[],[[]],[[0],[1,0]],[[2]],[[float('nan')]],[[0j]]]:
            with self.assertRaises(ValueError):board_potential(bad)

    def test_invalid_inputs_do_not_create_numeric_rewards(self):
        for kwargs in [dict(gamma=0),dict(gamma=1.1),dict(gamma=True),dict(gamma=10**1000),
                       dict(shaping_scale=-1),dict(line_weight=float('nan')),
                       dict(terminal_penalty=float('inf')),dict(time_basis='wall')]:
            with self.assertRaises((ValueError,TypeError)):RewardSpec(**kwargs)
        for lines,ticks,ended,before,after in [(-1,1,False,0,0),(0,0,False,0,0),
              (True,1,False,0,0),(0,1.0,False,0,0),(0,1,1,0,0),
              (0,1,False,float('inf'),0),(0,1,False,0,float('nan'))]:
            with self.assertRaises((ValueError,TypeError)):
                evaluate(lines,ticks,ended,before,after,RewardSpec())
        with self.assertRaises(ValueError):
            evaluate(4,1,False,0,0,RewardSpec(line_weight=1e308))


if __name__=='__main__':unittest.main()
