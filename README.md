# Predictive Input Buffer

An experimental Geode mod that checks a held jump input using a hidden player copy. If the simulated input reaches a fatal collision, the real input is held back and tested again on the next game update. A safe click is sent to the game immediately. The feature supports classic cube, robot, and wave modes; wave is simulated with jump held, and a release received while the press is queued is replayed after the press becomes safe.

## Settings

- **Maximum Delay (frames)**: Number of real game updates to wait for a safe trajectory. Default: 2. Set to 0 to disable. If no safe update appears before the limit, the queued input is dropped.
- **Trajectory Lookahead (frames)**: How far ahead the hidden player is advanced while holding the input. Default: 60. Larger values can catch later deaths but may add a short hitch when pressing.

Keyboard users can cycle the delay with F6. On iPad, change the settings from the Geode mod settings page.

## Prediction limits

The predictor is intentionally conservative. It only attempts a prediction for single-player, non-platformer cube/robot/wave play and ordinary static collision objects. If a nearby candidate is a trigger, grouped or animated object, or an unknown collision type, that probe is marked unsupported. Unsupported input at the initial press is passed through unchanged; an input already queued after a death prediction remains queued until a safe probe or its wait limit.

This is not a complete Geometry Dash physics/world simulator. It may miss hazards beyond the configured lookahead, level interactions, moving geometry, dual mode, and other special mechanics. The hidden clone uses the live PlayLayer's collision routines, guarded to suppress known trigger/death side effects; this has not been runtime-tested on an iPad. Treat the feature as experimental and test it on a practice run first.

## Target and build

The repository targets Geometry Dash 2.2081 and the Geode 5.11.2 SDK, including iOS. A literal Geode loader version 1.5.5 is not the SDK version used by this build configuration. The GitHub Actions workflow builds an iOS `.geode` artifact from the repository's `main` branch.
