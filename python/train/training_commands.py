"""Explicit experiment presets; budgets are examples, not difficulty guarantees."""
from pathlib import Path
import re
import shlex
import sys


def argv(command):
    result = shlex.split(command)
    result[0] = sys.executable
    return result


def command_for(algo: str, run_name: str, preset: str, checkpoint_dir) -> list[str]:
    if preset not in ("smoke", "long"):
        raise ValueError("preset must be smoke or long")
    if not re.fullmatch(r"[a-z0-9_-]{1,64}", run_name):
        raise ValueError("invalid run name")
    CHECKPOINT_DIR = Path(checkpoint_dir)
    smoke = preset == 'smoke'
    out = shlex.quote(str(CHECKPOINT_DIR / f'{run_name}.pt'))

    if algo == 'ppo':
        steps = 4096 if smoke else 1000000
        rollout = 512 if smoke else 2048
        return argv(f'python -m train.ppo_tetris --steps {steps} --rollout {rollout} --eval-every 1 --eval-episodes 1 --out {out}')
    if algo == 'ppo_sparse':
        steps = 4096 if smoke else 1000000
        rollout = 512 if smoke else 2048
        return argv(f'python -m train.ppo_tetris --steps {steps} --rollout {rollout} --shaping-coef 0 --eval-every 1 --eval-episodes 1 --out {out}')
    if algo in ('dqn', 'ddqn'):
        steps = 4096 if smoke else 500000
        warmup = 512 if smoke else 10000
        batch = 64 if smoke else 256
        eval_every = 2048 if smoke else 25000
        return argv(f'python -m train.dqn_tetris --target-mode {algo} --steps {steps} --warmup {warmup} --batch {batch} --eval-every {eval_every} --eval-episodes 1 --out {out}')
    if algo == 'cbmpi':
        iters = 2 if smoke else 20
        states = 512 if smoke else 20000
        epochs = 1 if smoke else 3
        return argv(f'python -m train.cbmpi_tetris --iterations {iters} --states-per-iter {states} --epochs {epochs} --batch 64 --eval-episodes 1 --out {out}')
    if algo == 'cbmpi_value':
        iters = 2 if smoke else 20
        states = 512 if smoke else 20000
        epochs = 1 if smoke else 3
        return argv(f'python -m train.cbmpi_tetris --iterations {iters} --states-per-iter {states} --epochs {epochs} --batch 64 --value-weight 0.25 --eval-episodes 1 --out {out}')
    if algo in ('reinforce', 'a2c', 'nstep_ac'):
        mode = 'nstep-ac' if algo == 'nstep_ac' else algo
        steps = 4096 if smoke else 500000
        rollout = 256 if algo == 'nstep_ac' else 512
        return argv(f'python -m train.policy_gradient_tetris --algo {mode} --steps {steps} --rollout {rollout} --eval-episodes 1 --out {out}')
    if algo == 'cem':
        iters = 2 if smoke else 50
        eps = 8 if smoke else 64
        pieces = 200 if smoke else 2000
        epochs = 1 if smoke else 3
        return argv(f'python -m train.cem_tetris --iterations {iters} --episodes-per-iter {eps} --max-pieces {pieces} --epochs {epochs} --out {out}')
    if algo == 'muzero':
        episodes = 4 if smoke else 500
        pieces = 100 if smoke else 1000
        sims = 4 if smoke else 32
        warmup = 16 if smoke else 2000
        batch = 16 if smoke else 256
        train_steps = 2 if smoke else 32
        distill = 20 if smoke else 2000
        return argv(f'python -m train.muzero_tetris --episodes {episodes} --max-pieces {pieces} --mcts-simulations {sims} --warmup {warmup} --batch {batch} --train-steps-per-episode {train_steps} --distill-steps {distill} --distill-batch {batch} --out {out}')
    raise ValueError(f'unknown ALGO: {algo}')
