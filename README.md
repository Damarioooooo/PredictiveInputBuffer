# Predictive Input Buffer

An iOS-compatible port of the approach in the attached `predictive-input-buffer` reference: simulate the current jump/button state, then test candidate delays one update at a time. If the immediate input dies but a candidate delay survives the required extra trajectory, the mod queues the real input for the earliest safe candidate frame.

For example, with **Maximum Delay** set to 2, only delays of 1 and 2 game updates are tested. If delay 2 is the first safe candidate, the input is replayed after two updates. A required delay of 3 is outside the configured window, so the mod passes the original input through immediately. That means it will not save an input that is too early for the selected window.

## Settings

- **Enable Predictive Buffer**: Turn buffering off or on without restarting. Turning it off flushes any queued button transition immediately.
- **Maximum Delay (frames)**: Select 1–8 game updates to search; default is 2.

Presses and releases are queued as button-state transitions. Wave prediction holds the candidate button state through the simulated trajectory; if the physical release arrives while a press is queued, the release is delivered after the buffered press.

## Prediction limits

This uses a hidden player copy and the live `PlayLayer` collision routines. It suppresses the known trigger/effect side effects during simulation. It currently bypasses buffering for dual, platformer, ship, and unsupported player contexts. The predictor uses a bounded static collision-object set and cannot fully reproduce every trigger-driven/moving-object interaction. In unsupported contexts input is passed through normally. Test in practice mode first.

The reference archive contains Windows-only console/key handling. This version ports its trajectory-and-candidate-delay logic without those Windows APIs and provides Geode settings suitable for iOS.

## Target and build

The repository targets Geometry Dash 2.2081 and Geode 5.11.2, including iOS. The iOS `.geode` artifact is built by the GitHub Actions workflow on `main`.
