import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True)
a=p.parse_args()
CP=Path(__file__).resolve().parents[1];sys.path.insert(0,str(CP/'python'))
from experiment import launch,hash_file


class ExperimentContract(unittest.TestCase):
    def test_native_cpu_worker_artifacts_and_duplicate(self):
        with tempfile.TemporaryDirectory() as directory:
            completed=launch(module_dir=a.module_dir,output_root=directory,name='smoke',preset='smoke')
            result=json.loads((completed/'result.json').read_text())
            self.assertEqual(result['status'],'success')
            self.assertEqual(result['checkpoint_hash'],hash_file(completed/'training.pt'))
            self.assertIn('worker completed 9',(completed/'train.log').read_text())
            manifest=json.loads((completed/'manifest.json').read_text())
            self.assertEqual(manifest['command'][0],sys.executable)
            with self.assertRaises(FileExistsError):
                launch(module_dir=a.module_dir,output_root=directory,name='smoke',preset='smoke')

    def test_invalid_or_unproven_run_does_not_start(self):
        with tempfile.TemporaryDirectory() as directory:
            for kwargs in [dict(name='../outside',preset='smoke'),dict(name='bad',preset='smkoe'),
                           dict(name='long',preset='long')]:
                with self.assertRaises(ValueError):launch(module_dir=a.module_dir,output_root=directory,**kwargs)
            self.assertEqual(list(Path(directory).iterdir()),[])

if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
