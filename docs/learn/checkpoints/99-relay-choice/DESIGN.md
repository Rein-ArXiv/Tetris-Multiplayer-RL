# 99 Relay choice

- Control/discovery, data path, rule authority and persistence are independent responsibilities.
- topology_probe uses real loopback TCP: one direct connection, or two outbound client connections accepted by a relay fixture.
- The single-thread blocking schedule owns all endpoints, exchanges only bounded frames, and has a CTest process timeout. It is not a general forwarding server.
- The relay knows the expected byte count from the fixture, copies opaque bytes, and does not authenticate or parse game state.
- Three ticks of RoundPlay are compared with full canonical state bytes, not only a hash. A future-round message still traverses the relay and is rejected by the endpoint.
- Each normal frame is18 bytes. Six frames give relay ingress/egress108 each; one extra invalid-round frame gives126 each. Direct relay counters stay0.
- read caps1/2/7/16 vary receive chunking independently of message boundaries. No WAN latency measurement is claimed.
- RelayDirection sums two assumed legs, relay wait and work in microseconds; asymmetric reverse direction is separate.
- RelayTraffic uses two clients per match and equal fixed frame rates/sizes; ingress equals egress under forward-once assumptions.
- Queue budgets are not resident memory; network overhead, retransmission, control, kernel buffers, auth, persistence and verification CPU are separate.
- Overflow is rejected before arithmetic; zero factors are handled before otherwise-overflowing products; failures preserve output.
- Root server retains frame filtering and optional authoritative replay; this fixture intentionally demonstrates the narrower transport responsibility.
