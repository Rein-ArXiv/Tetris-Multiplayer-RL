# Colab training and export environment

> 캐릭터별 모델·속도·일러스트와 서버 검증 BP는 [봇과 Colab 안내](../../docs/bots-and-colab.md)를 먼저 보세요.

The training side of Tetris-Multiplayer-RL lives in this directory. The
training and `.pt -> .onnx` export path runs on **Google Colab Linux x86_64
(GPU)**. Local deployment is inference-only: the C++ game loads exported
`model/*.onnx` and `model/bots/*.onnx` files with ONNX Runtime and does not
need PyTorch.

## Workflow

Use `train_model_zoo_colab.ipynb` as the training/export entry point. `setup_colab.ipynb`
only prepares the headless extension and runs a CPU integration probe.

1. Select the repository and optionally a full commit SHA. Existing checkouts are
   not pulled/reset automatically; preserve local changes before changing revision.
2. Install through the kernel's `sys.executable -m pip`. `prepare_native` selects
   that Python in CMake and builds in a new isolated directory. Existing build trees
   and loaded extensions are not deleted or overwritten.
3. Run the fresh-process smoke. It verifies the actual extension path/hash, native
   clone/step, observation/model update and model checkpoint round trip.
4. Select an explicit `smoke` experiment and a new RUN_NAME. Each run gets a directory
   with a command/source/environment manifest, streamed log, checkpoints and result record.
5. A `long` run requires SMOKE_RUN pointing to a successful matching algorithm/preparation.
   New source, dependencies or preparation require a new smoke. Use a different RUN_NAME.
6. Export from the completed run and download the opponent bundle. Deployment only
   needs inference assets and ONNX Runtime, not the training Python environment.

A notebook's saved cells are separate from kernel memory and runtime files.
With USE_DRIVE enabled, run directories (including logs/records) are stored below
MyDrive/tetris-checkpoints. A completed write is not a guarantee about remote
synchronization or power-loss durability. Interrupted runs without a result record
have an unknown outcome; inspect their logs and checkpoint before choosing a new run.

The current PPO/policy-gradient `--resume` option is a weights-only warm start:
new optimizer, RNG, environment and counters. Model files include constructor
configuration and explicit graph/I/O compatibility revisions. Full training resume
also needs optimizer, RNG, environment and collection/schedule state.

The notebook uses `train/colab_runtime.py`, `process_runner.py` and
`training_commands.py`; these can be tested without a remote Colab session. CPU
smoke is not GPU execution, ONNX validation, cross-platform parity or policy quality.

## Competitive versus environment

`common.env_versus.TetrisVersusEnv` provides a two-board, garbage-trading
Gymnasium environment. Its agent observation and placement mask match
`TetrisPlacementEnv`, so it can reuse `TetrisPolicyNet`. Available opponents
are `GreedyBCTSOpponent` (default), `RandomLegalOpponent`, and
`PolicyOpponent` for a frozen policy snapshot.

The PPO trainer selects the environment on the command line: `--env versus`
switches training and periodic evaluation to `TetrisVersusEnv` (the default is
`--env single`). The other trainer CLIs still instantiate `TetrisPlacementEnv`
directly; to train versus play with them, wire this environment into the
trainer or a wrapper explicitly.

Versus smoke test:

```bash
python -m train.ppo_tetris \
  --env versus \
  --steps 4096 \
  --rollout 512 \
  --eval-every 1 \
  --eval-episodes 1 \
  --out checkpoints/versus_smoke.pt
```

The environment's regression test is:

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
  `onnx`, `onnxscript`, and `onnxruntime` from `python/requirements-colab.txt`.
- Run export from `/content/Tetris-Multiplayer-RL/python`, or use
  `train_model_zoo_colab.ipynb`, which sets the working directory explicitly.
- If the message says `checkpoint not found`, finish the training cell first or
  export the latest `checkpoints/<run>.pt` instead of a missing
  `checkpoints/<run>.eval_best.pt`.
- If the native API is missing, prepare a new isolated build and repeat the fresh-process
  smoke. Compare the actual module path/hash; replacing a library file does not replace
  the module already loaded in a running kernel.

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
machine commands, not deployment-machine commands.

### DQN / Double DQN

`dqn_tetris.py` treats `TetrisPolicyNet.policy_logits` as Q-values over the
shared placement-action domain. `--target-mode dqn` uses the classic target-network max;
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
  policy opponents; selectable via `ppo_tetris.py --env versus`, still unwired
  in the other trainer CLIs.
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

## 모델 비교 기록

`train.model_zoo`를 준비한 native 모듈의 `train.colab_runtime launch` 경로로 실행한다.
같은 `--seeds`, `--max-pieces`, `--split validation` 조건에서 canonical .pt 또는
MuZero의 증류 .policy.pt를 비교하고 새로운 `--out` 경로에 JSON을 저장한다.
보고서의 개별 시드 결과·종료 이유·모델 해시를 확인한다. 후보 선택을 마친 뒤 별도로
정한 test 시드를 사용한다. 학습량·훈련 loss·알고리즘 이름은 난이도 등급이 아니다.
MuZero native 저장 파일은 직접 게임 모델로 평가하지 않는다.
DQN·MuZero·CEM·CBMPI의 --resume도 가중치 warm start이며 완전한 학습 재개가 아니다.

## Checked ONNX artifact

The exporter now checks fixed float32 I/O, embedded weights, ONNX structure,
and CPU Runtime outputs/greedy choices before replacing the destination.
It needs `onnxruntime` on the export machine. Default CLI cases are synthetic
tensor probes; `export(..., cases=...)` accepts actual observations with legal masks.
These finite checks do not establish playing strength or all-state equivalence.
Failed export/check/replace leaves the previous model intact; the input checkpoint
cannot also be the output path. C++ platform validation is still a separate step.
