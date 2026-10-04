import argparse
from copy import deepcopy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import torch
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
CP=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CP/'python'))
from training_checkpoint import TrainingRun,Config

torch.set_num_threads(1)


def equal_tree(test,a,b):
    if isinstance(a,torch.Tensor):
        test.assertTrue(torch.equal(a,b))
    elif isinstance(a,dict):
        test.assertEqual(a.keys(),b.keys())
        for key in a:equal_tree(test,a[key],b[key])
    elif isinstance(a,(tuple,list)):
        test.assertEqual(len(a),len(b))
        for x,y in zip(a,b):equal_tree(test,x,y)
    else:test.assertEqual(a,b)


class CheckpointContract(unittest.TestCase):
    def test_fresh_process_continuation_across_episode_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); run=TrainingRun()
            try:
                run.advance(); run.save(root/'saved.pt')
                self.assertGreater(run.collector.episode_decisions,0)
                self.assertTrue(run.optimizer.state)
                expected_data,expected_metrics=run.advance()
                run.save(root/'expected.pt')
                result=subprocess.run([sys.executable,str(CP/'python/checkpoint_demo.py'),
                    '--module-dir',a.module_dir,'--resume',str(root/'saved.pt'),
                    '--out',str(root/'resumed.pt')],check=True,capture_output=True,text=True,timeout=60)
                self.assertIn('checkpoint continuation complete',result.stdout)
                expected=torch.load(root/'expected.pt',weights_only=True)
                actual=torch.load(root/'resumed.pt',weights_only=True)
                equal_tree(self,expected,actual)
                evidence=torch.load(root/'resumed.evidence.pt',weights_only=True)
                equal_tree(self,expected_data,evidence['data'])
                equal_tree(self,expected_metrics,evidence['metrics'])
                self.assertGreater(len(run.collector.completed),0)
            finally: run.close()

    def test_failed_candidates_preserve_caller_rng(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'saved.pt';run=TrainingRun()
            try:
                run.advance();run.save(path)
                original=torch.load(path,weights_only=True)
                for case in ('version','rules','config','weights','adam','replay','rng','collector'):
                    saved=deepcopy(original)
                    if case=='version':saved['format_version']=True
                    elif case=='rules':saved['contract']['rules']='different'
                    elif case=='config':saved['config']['hidden']=True
                    elif case=='weights':saved['model'][next(iter(saved['model']))].fill_(float('nan'))
                    elif case=='adam':
                        first=next(iter(saved['optimizer']['state'].values()));first['exp_avg']=torch.zeros(1)
                    elif case=='replay':
                        raw=bytearray(saved['environment']['state_bytes']);raw[0]^=1
                        saved['environment']['state_bytes']=bytes(raw)
                    elif case=='rng':saved['torch_rng']=torch.zeros(1,dtype=torch.uint8)
                    else:saved['collector']['episode_decisions']+=1
                    torch.save(saved,path);before=torch.get_rng_state().clone()
                    with self.assertRaises((ValueError,RuntimeError,TypeError)):
                        TrainingRun.load(path)
                    self.assertTrue(torch.equal(before,torch.get_rng_state()),case)
            finally:run.close()

    def test_busy_or_failed_run_cannot_save(self):
        with tempfile.TemporaryDirectory() as directory:
            run=TrainingRun()
            try:
                run.ready=False
                with self.assertRaises(RuntimeError):run.save(Path(directory)/'bad.pt')
                run.ready=True
                with patch('training_checkpoint.update',side_effect=ValueError('injected')):
                    with self.assertRaises(ValueError):run.advance()
                with self.assertRaises(RuntimeError):run.save(Path(directory)/'bad.pt')
                with self.assertRaises(RuntimeError):run.advance()
            finally:run.close()

    def test_initial_snapshot_and_partial_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'initial.pt';run=TrainingRun(Config(conv_channels=(2,),hidden=8))
            try:
                run.save(path); restored=TrainingRun.load(path)
                try:
                    restored.advance(1)
                    self.assertEqual(restored.decisions,1)
                    self.assertEqual(restored.model.conv_channels,(2,))
                finally:restored.close()
            finally:run.close()

if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
