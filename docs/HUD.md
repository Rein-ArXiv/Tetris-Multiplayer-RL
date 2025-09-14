# HUD / UI Overview (Learning Notes)

This project shows simple yet informative HUD elements to understand the netcode.

- Single mode
  - Title + Score + Next block preview (classic Tetris UI)
- Net mode
  - Two boards: left(Local), right(Remote)
  - Score per board
  - Next block preview per board
  - Status line (bottom-left):
    - `NET: CONNECTED/DISCONNECTED`
    - When ready: `SEED: 0x...`
    - Ticks diagnostic: `localSent`(last local input tick sent), `remoteMax`(last remote input tick seen), `sim`(last simulated tick), `delay`(input delay)
  - Hash line (bottom-right):
    - `HASH remote: t=..., h=0x...` → last remote state hash received (sent every ~30 ticks)
  - Waiting messages:
    - `Waiting for connection...` or `Waiting for session ready...`

Notes
- The “hash” is currently a learning aid: it shows the other side’s reported state hash. Since each board legitimately differs, hashes need not match; the purpose is to exercise framing, periodic messages, and on‑screen diagnostics.
- The “safe tick” is approximated as `min(localSent, remoteMax) - inputDelay`. Simulation proceeds up to that tick.
- You can adjust input delay or add PING/PONG RTT measurements to tune it later.

Keyboard
- Global: `H` prints state hashes to console
- Replay: `F5` start, `F6` save to `out/replay.txt`
- Menu: ↑/↓ navigate; Enter select; Connect screen supports number, dot, colon; Backspace delete; ESC back

