# Colab training and export environment

The training side of Tetris-Multiplayer-RL lives in this directory. The
training and `.pt -> .onnx` export path runs on **Google Colab Linux x86_64
(GPU)**. Local deployment is inference-only: the C++ game loads the exported
`model/*.onnx` with ONNX Runtime and does not need PyTorch.

The Python `netbot` can still run a `.pt` checkpoint for debugging, but that
path imports PyTorch. On low-power deployment machines, export in Colab and
copy only the `.onnx` file.

## Workflow

1. Open `setup_colab.ipynb` in Colab.
2. Set `REPO_URL` to your fork.
3. Run all cells. This clones the repo, builds `tetris_py.so` from the same
   C++ sources used by the Windows game, and verifies that
   `from sim import SimGame` and `from common.models import TetrisPolicyNet`
   import cleanly.
4. Run the baseline PPO loop, or open your own training notebook
   (`train_muzero.ipynb`, `train_dqn.ipynb`, etc.) and
   `import sys; sys.path.insert(0, '/content/Tetris-Multiplayer-RL/python')`
   so it picks up the freshly built `sim` module and the shared `common`
   layer.
5. Save checkpoints with `common.checkpoint.save_checkpoint(...)` or the
   built-in PPO trainer. Checkpoints embed the architecture version so the
   local netbot's `load_checkpoint` refuses stale architectures with a
   `RuntimeError` instead of silently loading wrong-shape weights.
6. Export `.pt` to `.onnx` in Colab, download it, and put only the result
   under local `model/` for the in-game bot roster.

## Baseline PPO

Quick smoke test:

```bash
cd /content/Tetris-Multiplayer-RL/python
python -m train.ppo_tetris \
  --steps 4096 \
  --rollout 512 \
  --eval-every 1 \
  --eval-episodes 1 \
  --eval-max-pieces 500 \
  --out checkpoints/smoke.pt
```

First useful baseline run:

```bash
cd /content/Tetris-Multiplayer-RL/python
python -m train.ppo_tetris \
  --steps 1000000 \
  --out checkpoints/aria_ppo_baseline.pt \
  --eval-every 10 \
  --eval-episodes 5
```

The trainer writes:

- `checkpoints/aria_ppo_baseline.pt` — latest policy
- `checkpoints/aria_ppo_baseline.best.pt` — best shaped training return
- `checkpoints/aria_ppo_baseline.eval_best.pt` — best greedy evaluation result

Export one checkpoint to an in-game bot model. This command uses PyTorch, so
run it in Colab or another training/export machine:

```bash
python -m netbot.export_onnx \
  checkpoints/aria_ppo_baseline.eval_best.pt \
  ../model/aria_ppo_baseline.onnx
```

## Cross-platform determinism gate

Before trusting any training run, verify that Linux and Windows produce
**bitwise-identical** state hashes. The C++ test driver
`build/sim_hash_dump` is the ground truth.

On Colab (cell 5 of the setup notebook does this):

```bash
build/sim_hash_dump > python/tests/_sim_hash_dump.txt
```

On Windows (after building locally):

```cmd
build\Release\sim_hash_dump.exe > python\tests\_sim_hash_dump.windows.txt
```

`diff` the two files. They must be byte-identical. If they're not, the RNG
or hash code has a platform-dependent bug (most likely `int` width or
unsigned modulo behaviour) and any policy trained on Colab will desync
when deployed to the Windows netbot.

## What goes where

- `setup_colab.ipynb` — environment bootstrap (run first, every cold runtime).
- `README_colab.md`   — this file.
- `ppo_tetris.py` — baseline legal-action-masked PPO trainer.
- `train_ppo_colab.ipynb` — notebook wrapper around the baseline PPO trainer.
- *Your* training notebooks — keep them in this directory so they're version-
  controlled with the code they depend on. They should `import` from
  `common` (architecture, obs, checkpoint) and from `sim` (the env).

## Frameworks

The plan deliberately leaves the training algorithm and library choice open.
The shared layer (`common/`) only fixes:

- the network architecture (`TetrisPolicyNet`)
- the observation format (`build_observation`)
- the checkpoint format (`save_checkpoint` / `load_checkpoint`)
- a Gymnasium env wrapper (`common.env.TetrisPlacementEnv`) so SB3 / CleanRL /
  LightZero / RLlib can plug in without bespoke glue

Pick whatever lets you iterate fastest. MuZero on LightZero, PPO on CleanRL,
or hand-rolled PyTorch — they all read the same env and write the same
checkpoint format, so the netbot doesn't care which one trained the model.
