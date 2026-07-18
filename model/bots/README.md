# Bot model roster

Put exported ONNX policies here to make them appear separately in
`Single vs Bot`. The game must be built with `TETRIS_BUILD_BOT=ON` and a
compatible ONNX Runtime bundle to load them. The built-in heuristic entry is
available without either dependency.

Example layout:

```text
model/bots/aria_ppo.onnx
model/bots/aria_ppo_sparse.onnx
model/bots/aria_dqn.onnx
model/bots/aria_ddqn.onnx
model/bots/aria_cbmpi.onnx
model/bots/aria_a2c.onnx
model/bots/aria_reinforce.onnx
model/bots/aria_nstep_ac.onnx
model/bots/aria_cem.onnx
model/bots/aria_muzero.onnx
```

The game also scans legacy `model/*.onnx`, so `model/policy.onnx` still works.
For many bots, prefer `model/bots/*.onnx` to keep the root `model/` directory
readable.

Optional display names and default speeds live in `model/bots.cfg`:

```text
# path-or-filename|display name|input_interval_ticks
model/bots/aria_ppo.onnx|Aria PPO|1
model/bots/aria_ppo_sparse.onnx|Aria PPO Sparse|2
model/bots/aria_dqn.onnx|Aria DQN|2
model/bots/aria_ddqn.onnx|Aria DDQN|2
aria_cbmpi.onnx|Aria CBMPI|3
model/bots/aria_a2c.onnx|Aria A2C|2
model/bots/aria_reinforce.onnx|Aria REINFORCE|3
model/bots/aria_nstep_ac.onnx|Aria n-step AC|2
model/bots/aria_cem.onnx|Aria CEM|2
model/bots/aria_muzero.onnx|Aria MuZero|3
@heuristic|Heuristic (slow)|2
```

`input_interval_ticks` controls how often the in-game bot consumes one queued
frame input. `1` is the old behavior: one bot input every simulation tick.
Higher values slow the bot down without changing the model.

Debug builds with `TETRIS_ENABLE_DEBUG_UI` can temporarily adjust speed on the
bot selection screen and during `Single vs Bot`. Release builds use the
configured/default interval only.
