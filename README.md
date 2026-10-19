# Predictive Input Buffer

An experimental Geode mod that checks a held jump input using a hidden player copy. A tap is tested as if it were held from the current frame. If that trajectory reaches a fatal collision, the real input is held back and tested again on each game update. The input is replayed only when a later probe reports a safe path. The feature supports classic cube, robot, and wave modes; wave simulation keeps the button held for the full prediction horizon. A release received while the press is queued is replayed after the press becomes safe.

## Settings

- **Enable Predictive Buffer**: Turn prediction on or off without restarting. Turning it off immediately sends any queued input through the game.
- **Trajectory Lookahead (frames)**: How far ahead the hidden player is advanced while holding the input to test whether the path dies. Default: 12. Larger values can catch later deaths but increase simulation work.

On iPad, use the Enable switch and adjust lookahead from the Geode mod settings page. A tap that was identified as deadly stays queued until a probe is safe, unless you turn the mod off; restarting or leaving the level also clears pending input.

## Prediction limits

The predictor is intentionally conservative. It only attempts a prediction for single-player, non-platformer cube/robot/wave play and ordinary static collision objects. If a nearby candidate is a trigger, grouped or animated object, or an unknown collision type, that probe is marked unsupported. Unsupported input at the initial press is passed through unchanged; an input already queued after a death prediction remains queued until a safe probe or its wait limit.

This is not a complete Geometry Dash physics/world simulator. It may miss hazards beyond the configured lookahead, level interactions, moving geometry, dual mode, and other special mechanics. The hidden clone uses the live PlayLayer's collision routines, guarded to suppress known trigger/death side effects; this has not been runtime-tested on an iPad. If a queued probe remains inconclusive or the level has no predicted-safe frame, the input can stay queued until you restart or leave the level. Test it on a practice run first.

## Target and build

The repository targets Geometry Dash 2.2081 and the Geode 5.11.2 SDK, including iOS. A literal Geode loader version 1.5.5 is not the SDK version used by this build configuration. The GitHub Actions workflow builds an iOS `.geode` artifact from the repository's `main` branch.
