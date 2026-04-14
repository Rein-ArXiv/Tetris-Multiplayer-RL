# Colab training environment

The training side of Tetris-Multiplayer-RL lives in this directory. The
production deployment runs on **Google Colab Linux x86_64 (GPU)**; the local
Windows machine is **inference-only** via `python/netbot/`.

## Workflow

1. Open `setup_colab.ipynb` in Colab.
2. Set `REPO_URL` to your fork.
3. Run all cells. This clones the repo, builds `tetris_py.so` from the same
   C++ sources used by the Windows game, and verifies that
   `from sim import SimGame` and `from common.models import TetrisPolicyNet`
   import cleanly.
4. Open your training notebook (your own — `train_muzero.ipynb` etc.) and
   `import sys; sys.path.insert(0, '/content/Tetris-Multiplayer-RL/python')`
   so it picks up the freshly built `sim` module and the shared `common`
   layer.
5. Train. Use `common.checkpoint.save_checkpoint(model, '/content/run.pt')`
   to write checkpoints — that file embeds the architecture version so the
   local netbot's `load_checkpoint` will refuse a stale arch by raising
   `RuntimeError` instead of silently loading wrong-shape weights.
6. Download the `.pt` to your Windows machine and run
   `python -m netbot.client --connect 127.0.0.1:7777 --policy run.pt`.

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
