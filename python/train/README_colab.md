# Colab training and export environment

The training side of Tetris-Multiplayer-RL lives in this directory. The
training and `.pt -> .onnx` export path runs on **Google Colab Linux x86_64
(GPU)**. Local deployment is inference-only: the C++ game loads exported
`model/*.onnx` and `model/bots/*.onnx` files with ONNX Runtime and does not
need PyTorch.

## Workflow

`train_model_zoo_colab.ipynb` is the single entry point. Open it in Colab (or
the VSCode Colab extension) and run from the top:

1. Set `REPO_URL` to your fork and `ALGO` to the algorithm you want.
2. The setup cells clone the repo, install deps from
   `python/requirements-colab.txt`, build `tetris_py.so` from the same C++
   sources used by the game, and run an import smoke test
   (`from sim import SimGame`, `from common.models import TetrisPolicyNet`).
3. The remaining cells train, export `.onnx`, generate `model/bots.cfg`, and
   download the artifacts.
4. Put the downloaded `.onnx` under local `model/bots/` for the in-game bot
   roster. The game still scans legacy `model/*.onnx`, but `model/bots/*.onnx`
   is the preferred layout when you have many models.

Checkpoints embed the architecture version, so `load_checkpoint` (used by the
export CLI) refuses stale architectures with a `RuntimeError` instead of
silently loading wrong-shape weights.

To run trainers manually instead, execute any command from the sections below
inside the notebook runtime (after the setup cells), or in your own notebook
after `import sys; sys.path.insert(0, '/content/Tetris-Multiplayer-RL/python')`.

## Competitive versus environment

`common.env_versus.TetrisVersusEnv` provides a two-board, garbage-trading
Gymnasium environment. Its agent observation and 40-action mask match
`TetrisPlacementEnv`, so it can reuse `TetrisPolicyNet`. Available opponents
are `GreedyBCTSOpponent` (default), `RandomLegalOpponent`, and
`PolicyOpponent` for a frozen policy snapshot.

The current trainer CLIs instantiate `TetrisPlacementEnv` directly; selecting
`TetrisVersusEnv` is not yet a command-line option. To train versus play, wire
this environment into a trainer or wrapper explicitly. The regression test is:

```bash
python -m pytest tests/test_versus_env.py -q
```

It requires the built `tetris_py` module and Gymnasium; otherwise pytest skips
the module.

## Export Troubleshooting

`CalledProcessError` from an ONNX export cell means the subprocess failed; the
real cause is in that subprocess's stdout/stderr. The current notebooks print
both before raising.

Common fixes:

- Re-run the setup/dependency cell after `git pull`. It installs `torch`,
  `onnx`, and `onnxscript` from `python/requirements-colab.txt`.
- Run export from `/content/Tetris-Multiplayer-RL/python`, or use
  `train_model_zoo_colab.ipynb`, which sets the working directory explicitly.
- If the message says `checkpoint not found`, finish the training cell first or
  export the latest `checkpoints/<run>.pt` instead of a missing
  `checkpoints/<run>.eval_best.pt`.
- If `SimGame.clone()` is missing, delete `build/` and `python/sim/tetris_py*.so`
  and rebuild the native module. The setup notebook already does this.

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
  ../model/bots/aria_ppo_baseline.onnx
```

## Additional Algorithms

All commands below run from `/content/Tetris-Multiplayer-RL/python` after the
zoo notebook's setup cells have built `tetris_py`. They are Colab/training-
machine commands, not Mac mini deployment commands.

### DQN / Double DQN

`dqn_tetris.py` treats `TetrisPolicyNet.policy_logits` as Q-values over the
40 placement actions. `--target-mode dqn` uses the classic target-network max;
`--target-mode ddqn` uses Double DQN online-argmax/target-gather targets. The
value head is unused. The saved `.pt` is still a canonical `TetrisPolicyNet`
checkpoint, so export is identical to PPO.

Smoke test:

```bash
python -m train.dqn_tetris \
  --target-mode ddqn \
  --steps 4096 \
  --warmup 512 \
  --batch 64 \
  --eval-every 2048 \
  --eval-episodes 1 \
  --out checkpoints/dqn_smoke.pt
```

Longer run:

```bash
python -m train.dqn_tetris \
  --target-mode ddqn \
  --steps 500000 \
  --out checkpoints/aria_dqn.pt \
  --eval-every 25000 \
  --eval-episodes 5

python -m netbot.export_onnx \
  checkpoints/aria_dqn.eval_best.pt \
  ../model/bots/aria_dqn.onnx
```

Classic DQN is the same trainer with a different target:

```bash
python -m train.dqn_tetris \
  --target-mode dqn \
  --steps 500000 \
  --out checkpoints/aria_dqn_classic.pt

python -m netbot.export_onnx \
  checkpoints/aria_dqn_classic.eval_best.pt \
  ../model/bots/aria_dqn_classic.onnx
```

### CBMPI

`cbmpi_tetris.py` alternates between one-step policy improvement and supervised
policy fitting. The improvement step clones the current `SimGame`, applies
each legal placement, and scores the post-placement board with BCTS features.
`--value-weight` can add the current network's value estimate as a bootstrap;
the default is `0.0` so the first iteration is not polluted by an untrained
value head.

This trainer requires the current pybind11 module because it uses
`SimGame.clone()`. If Colab reports that `clone` is missing, rerun the setup
notebook so `tetris_py` is rebuilt from the current repository.

Smoke test:

```bash
python -m train.cbmpi_tetris \
  --iterations 2 \
  --states-per-iter 512 \
  --epochs 1 \
  --batch 64 \
  --eval-episodes 1 \
  --out checkpoints/cbmpi_smoke.pt
```

Longer run:

```bash
python -m train.cbmpi_tetris \
  --iterations 20 \
  --states-per-iter 20000 \
  --out checkpoints/aria_cbmpi.pt \
  --eval-episodes 5

python -m netbot.export_onnx \
  checkpoints/aria_cbmpi.eval_best.pt \
  ../model/bots/aria_cbmpi.onnx
```

### Policy Gradient Family

`policy_gradient_tetris.py` provides three deployable policy-gradient
baselines:

- `reinforce` — Monte-Carlo policy gradient with value baseline.
- `a2c` — synchronous advantage actor-critic.
- `nstep-ac` — actor-critic with shorter n-step rollouts.

Smoke test:

```bash
python -m train.policy_gradient_tetris \
  --algo a2c \
  --steps 4096 \
  --rollout 512 \
  --eval-episodes 1 \
  --out checkpoints/a2c_smoke.pt
```

Longer runs:

```bash
python -m train.policy_gradient_tetris \
  --algo reinforce \
  --steps 300000 \
  --out checkpoints/aria_reinforce.pt

python -m train.policy_gradient_tetris \
  --algo a2c \
  --steps 500000 \
  --out checkpoints/aria_a2c.pt

python -m train.policy_gradient_tetris \
  --algo nstep-ac \
  --steps 500000 \
  --rollout 256 \
  --out checkpoints/aria_nstep_ac.pt

python -m netbot.export_onnx checkpoints/aria_reinforce.eval_best.pt ../model/bots/aria_reinforce.onnx
python -m netbot.export_onnx checkpoints/aria_a2c.eval_best.pt ../model/bots/aria_a2c.onnx
python -m netbot.export_onnx checkpoints/aria_nstep_ac.eval_best.pt ../model/bots/aria_nstep_ac.onnx
```

### Cross-Entropy Method

`cem_tetris.py` samples episodes, keeps the top return percentile, and trains
the policy by cross-entropy on elite actions. It is a good low-assumption
baseline because it does not depend on bootstrapped Q targets.

Smoke test:

```bash
python -m train.cem_tetris \
  --iterations 2 \
  --episodes-per-iter 8 \
  --max-pieces 200 \
  --epochs 1 \
  --out checkpoints/cem_smoke.pt
```

Longer run:

```bash
python -m train.cem_tetris \
  --iterations 50 \
  --episodes-per-iter 64 \
  --out checkpoints/aria_cem.pt

python -m netbot.export_onnx \
  checkpoints/aria_cem.eval_best.pt \
  ../model/bots/aria_cem.onnx
```

### MuZero-style

`muzero_tetris.py` trains a separate MuZero-style model with representation,
dynamics, and prediction heads, then distills the MCTS visit targets into the
canonical `TetrisPolicyNet`.

There are two outputs:

- `checkpoints/aria_muzero.pt` — native MuZero-style checkpoint for continued
  MuZero training only.
- `checkpoints/aria_muzero.policy.pt` — deployable `TetrisPolicyNet`
  checkpoint produced by distillation.

Export only the `.policy.pt` checkpoint.

Smoke test:

```bash
python -m train.muzero_tetris \
  --episodes 4 \
  --max-pieces 100 \
  --mcts-simulations 4 \
  --warmup 16 \
  --batch 16 \
  --train-steps-per-episode 2 \
  --distill-steps 20 \
  --distill-batch 16 \
  --out checkpoints/muzero_smoke.pt
```

Longer run:

```bash
python -m train.muzero_tetris \
  --episodes 500 \
  --mcts-simulations 32 \
  --out checkpoints/aria_muzero.pt \
  --distill-steps 2000

python -m netbot.export_onnx \
  checkpoints/aria_muzero.policy.pt \
  ../model/bots/aria_muzero.onnx
```

## Cross-platform determinism gate

Before trusting any training run, verify that Linux and Windows produce
**bitwise-identical** state hashes. The C++ test driver
`build/sim_hash_dump` is the ground truth.

On Colab (the zoo notebook's smoke-test cell does this):

```bash
build/sim_hash_dump > python/tests/_sim_hash_dump.txt
```

On Windows (after building locally):

```cmd
build\Release\sim_hash_dump.exe > python\tests\_sim_hash_dump.windows.txt
```

`diff` the two files. They must be byte-identical. If they're not, the RNG
or hash code has a platform-dependent bug (most likely `int` width or
unsigned modulo behaviour) and any policy trained on Colab will behave
differently when deployed to the in-game ONNX bot.

## What goes where

- `README_colab.md`   — this file.
- `setup_colab.ipynb` — standalone Colab bootstrap and native-module build.
- `train_model_zoo_colab.ipynb` — the single Colab notebook: environment
  bootstrap + training + export + roster generation for any supported
  algorithm.
- `ppo_tetris.py` — baseline legal-action-masked PPO trainer.
- `dqn_tetris.py` — Double DQN trainer; writes deployable policy checkpoints.
- `cbmpi_tetris.py` — BCTS/value-improved CBMPI-style trainer; writes
  deployable policy checkpoints.
- `policy_gradient_tetris.py` — REINFORCE, A2C, and n-step actor-critic.
- `cem_tetris.py` — Cross-Entropy Method policy-search baseline.
- `muzero_tetris.py` — MuZero-style trainer plus policy distillation.
- `rl_common.py` — shared batching, masking, evaluation, and replay helpers.
- `../common/env_versus.py` — two-board garbage environment and scripted/
  policy opponents; not yet selected by the built-in trainer CLIs.
- `../../model/bots/README.md` — in-game bot roster layout and speed metadata.
- *Your* training notebooks — keep them in this directory so they're version-
  controlled with the code they depend on. They should `import` from
  `common` (architecture, obs, checkpoint) and from `sim` (the env).

## Frameworks

The shared layer (`common/`) fixes the deployment contract:

- the network architecture (`TetrisPolicyNet`)
- the observation format (`build_observation`)
- the checkpoint format (`save_checkpoint` / `load_checkpoint`)
- a Gymnasium env wrapper (`common.env.TetrisPlacementEnv`) so SB3 / CleanRL /
  LightZero / RLlib can plug in without bespoke glue

PPO, DQN, and CBMPI in this directory train `TetrisPolicyNet` directly. MuZero
uses a native MuZero-style model for training and writes a deployable
`TetrisPolicyNet` only through the distillation step. External frameworks are
still fine as long as their final export path writes the canonical checkpoint
or an ONNX file with the same input/output contract.
