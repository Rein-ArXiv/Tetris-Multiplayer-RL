"""Actual native policies, paired records, cutoff and RNG ownership."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
p=argparse.ArgumentParser();p.add_argument('--module-dir',required=True);args,rest=p.parse_known_args()
MODULE=Path(args.module_dir).resolve()
CP=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(MODULE),str(CP/'python')]
import study_py
import torch
from training_checkpoint import TrainingRun
from model_zoo import compare
from evaluation_summary import summarize,paired_difference

torch.set_num_threads(1)


class Contract(unittest.TestCase):
    def test_native_comparison_and_child_cli(self):
        with tempfile.TemporaryDirectory() as folder:
            folder=Path(folder);a=folder/'initial.pt';b=folder/'updated.pt'
            run=TrainingRun()
            try:
                run.save(a);run.advance(3);run.save(b)
            finally:run.close()
            before=torch.get_rng_state().clone()
            report=compare([a,b],seeds=[21,32],limit=2,module_dir=MODULE,split='validation')
            self.assertTrue(torch.equal(before,torch.get_rng_state()))
            self.assertEqual(report['candidates'][0]['training_decisions'],0)
            self.assertEqual(report['candidates'][1]['training_decisions'],3)
            for candidate in report['candidates']:
                self.assertEqual([r['seed'] for r in candidate['rows']],[21,32])
                self.assertTrue(all(r['end']=='budget' and r['pieces']==2 for r in candidate['rows']))
            second=compare([a,b],seeds=[21,32],limit=2,module_dir=MODULE,split='validation')
            self.assertEqual(report,second)
            same=folder/'same.pt';same.write_bytes(a.read_bytes())
            equal=compare([a,same],seeds=[21,32],limit=2,module_dir=MODULE,split='test')
            self.assertEqual(equal['differences'][0]['result']['tied_seeds'],2)
            command=[sys.executable,str(CP/'python/model_zoo.py'),str(a),str(b),
                     '--module-dir',str(MODULE),'--seeds','21','32','--limit','2',
                     '--split','validation','--out',str(folder/'report.json')]
            result=subprocess.run(command,capture_output=True,text=True,timeout=45)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(json.loads((folder/'report.json').read_text())['candidates'],report['candidates'])
            repeated=subprocess.run(command,capture_output=True,text=True,timeout=45)
            self.assertNotEqual(repeated.returncode,0)

    def test_protocol_and_bad_measurements(self):
        with self.assertRaises(ValueError):compare([],seeds=[1],limit=2,module_dir=MODULE,split='validation')
        with self.assertRaises(ValueError):compare(['a'],seeds=[1,1],limit=2,module_dir=MODULE,split='validation')
        row=dict(seed=1,lines=2,score=5,pieces=3,reward=99.,end='budget')
        self.assertEqual(summarize([row])['metrics']['lines']['mean'],2)
        self.assertEqual(summarize([row])['metrics']['reward']['mean'],99.)
        with self.assertRaises(ValueError):summarize([{**row,'reward':float('nan')}])
        with self.assertRaises(ValueError):paired_difference([row],[{**row,'seed':2}])
        other={**row,'lines':5}
        self.assertEqual(paired_difference([row],[other])['mean'],3)


if __name__=='__main__':unittest.main(argv=[sys.argv[0],*rest])
