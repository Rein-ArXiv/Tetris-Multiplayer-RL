# Legacy Pygame implementation

This directory contains the original pure-Python Pygame port of the C++
Tetris game. It is **not** part of the RL pipeline — it does not share state
with the C++ simulator, has no networking, and produces a different state
hash.

It's kept here as a reference implementation for the gameplay rules. The
authoritative game logic now lives in `src/sim_game.{h,cpp}` (C++) and is
exposed to Python via `from sim import SimGame`.

If you want to play the legacy version standalone:

```bash
python python/legacy/main.py
```

(requires `pygame` — not in `requirements.txt`).
