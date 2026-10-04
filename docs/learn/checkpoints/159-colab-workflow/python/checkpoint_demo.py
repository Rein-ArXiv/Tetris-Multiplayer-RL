"""Fresh-process continuation probe; output is evidence data, not a trained bot."""
import argparse
from pathlib import Path
import sys
import torch
p=argparse.ArgumentParser()
p.add_argument('--module-dir',required=True)
p.add_argument('--resume',required=True)
p.add_argument('--out',required=True)
a=p.parse_args();sys.path.insert(0,a.module_dir)
from training_checkpoint import TrainingRun

torch.set_num_threads(1)
run=TrainingRun.load(a.resume)
try:
    data,metrics=run.advance()
    run.save(a.out)
    torch.save({'data':data,'metrics':metrics},Path(a.out).with_suffix('.evidence.pt'))
    print('checkpoint continuation complete',run.decisions,run.updates)
finally:
    run.close()
