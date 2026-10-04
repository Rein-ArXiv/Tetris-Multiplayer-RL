import argparse
import sys
import numpy as np
from observation import observe, stack_batch, to_tensors

parser = argparse.ArgumentParser()
parser.add_argument("--module-dir", required=True)
args = parser.parse_args()
sys.path.insert(0, args.module_dir)
import study_py as sim

schema = sim.observation_schema()
first = sim.Session(77, 2)
second = first.clone()
second.step(0)  # Only a hidden gravity counter changes on this tick.
a, b = observe(first, schema), observe(second, schema)
print("same observation:", all(np.array_equal(a[k], b[k]) for k in a))
print("different full state:", first.state_bytes() != second.state_bytes())
batch = stack_batch([a, b])
tensors = to_tensors(batch)
for name, tensor in tensors.items():
    print(name, tuple(tensor.shape), tensor.dtype, tensor.device, tensor.is_contiguous())
tensors["board"][0, 0, 0, 0] = 1
print("NumPy shares tensor storage:", batch["board"][0, 0, 0, 0] == 1)
print("source observation is independent:", a["board"][0, 0, 0] == 0)
print("simulation is independent:", first.grid()[0][0] == 0)
