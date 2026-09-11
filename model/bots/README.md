# Bot model roster

Current character, pacing, Colab and BP guide: [docs/bots-and-colab.md](../../docs/bots-and-colab.md).

Put trained policies in `model/bots/*.onnx` and register characters in `assets/opponents.cfg`:

```text
aria|Aria|model/bots/aria_ppo.onnx|assets/icons/bot.png|assets/icons/bot.png|Normal|6|18|60
```

The three timing values are input interval, thinking delay and minimum hard-drop age,
in 60Hz ticks. `6|18|60` gives 0.1s inputs, 0.3s thinking and at least 1s per piece.
Gravity is unchanged. Models can be shared by several characters with distinct IDs.

The default Lumen/Rook/Vega entries are the same built-in heuristic with different
speeds and placeholder images. They are available without ONNX Runtime. Learned
policies require `TETRIS_BUILD_BOT=ON` on clients and the optional BP verifier.

Legacy auto-discovery of `model/*.onnx` and `model/bots/*.onnx` remains supported.
For unregistered models only, `model/bots.cfg` can set
`path|name|input_ticks[|think_ticks|min_piece_ticks]`. Explicit character profiles take precedence.

Deploy the same catalog and model files to clients and the meta server. Only
server-verified online wins can earn common shop BP (`tetris_meta --bot-rewards`).
Offline practice awards none. Training and export stay in Colab; runtime uses CPU inference.
