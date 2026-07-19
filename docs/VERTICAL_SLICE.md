# Initial Vertical Slice

This document records the original playable baseline target. The current prototype has moved past
this plan: it now includes a larger seeded asteroid field, eight development-strength drones,
dual particle shots, Gravity Sling, cargo hauling and extraction, raider task behavior, combat
pressure escalation, and broader HUD/read-model coverage. Use this file as historical acceptance
context; use `README.md`, `TODO.md`, and `docs/SYSTEMS.md` for current state and next work.

## Goal

Produce a playable Linux and Steam Deck-oriented prototype that proves:

- assisted gamepad flight feels good
- the wraparound sector is readable
- camera lag feels good
- target lock is useful
- the HUD can expose dense information clearly
- mining can feel like combat against a rock
- one autonomous drone can contribute meaningfully
- quota and escalation create pressure
- a cargo box can be stolen, chased, recovered, and reattached

## Milestone 0: Repository Bootstrap

Deliver:

- C++23 CMake project
- CPM dependency setup
- SDL3 window and gamepad initialization
- Dawn/WebGPU renderer
- Catch2 test target
- Linux build path
- MSYS2 build path
- Steam Deck notes
- warnings-as-errors in project code
- formatting configuration
- basic CI if repository hosting is ready

Acceptance:

- application launches
- controller is detected
- test binary runs
- clean build from documented commands

## Milestone 1: Flight Laboratory

Deliver:

- fixed simulation timestep
- semantic input frame
- desired movement
- assisted acceleration and braking
- ship rotation and aim
- 9x9 sector dimensions
- wraparound movement
- wrapped-distance helpers
- camera anchor at approximately 75 percent screen height
- positional and rotational camera lag
- minimal flight HUD

Acceptance:

- ship feels controllable on a gamepad
- crossing a sector edge is seamless
- camera parameters can be tuned without recompiling
- wrapped distance has tests

## Milestone 2: Asteroid Relationship

Deliver:

- one large asteroid
- target selection
- lock and unlock state
- relative position and velocity
- target approach information
- placeholder scan data
- target HUD brackets
- explicit operational mode indication

Acceptance:

- player can approach, lock, orbit, leave, and reacquire
- current control context is visible
- lock state transitions have tests

## Milestone 3: Combat Against a Rock

Deliver:

- mining laser
- one ore seam
- one heat value
- one structural stress value
- one volatile gas pocket
- extracted material production
- exaggerated mining HUD
- hit feedback and gamepad feedback

Acceptance:

- reckless mining can trigger a bad result
- careful mining produces more material
- HUD explains why
- mining behavior is tunable

## Milestone 4: Drone and Cargo

Deliver:

- one autonomous mining drone
- player-marked mining priority
- remote extraction point
- cargo box creation
- quota tracking
- extraction authorization
- over-quota bonus
- 1-minute development escalation hook, with 5-minute contract timing retained as the design target

Acceptance:

- drone contributes without direct piloting
- cargo reaches the extraction point
- quota status is always clear
- escalation state is visible

## Milestone 5: Escort Slice

Deliver:

- modular cargo train
- rear propulsion module
- beacon-plotted route
- autoscrolling escort phase
- one raider
- 0.5-second box coupling disruption
- one-box tow slot
- wrapped escape distance
- 4.5-screen escape threshold
- player recovery tow
- box return and reconnection
- maximum-juice stolen-cargo HUD

Acceptance:

- raider can steal one box
- player can intercept and recover it
- raider can escape with it
- loss is partial, visible, and understandable
- tow stress can break the player's recovery link
- all theft lifecycle transitions have tests

## First Playtest Questions

- Does movement feel competent rather than slippery?
- Does camera lag create useful look-ahead without nausea?
- Is target lock useful before deeper mining systems exist?
- Can the player understand the asteroid as an opponent?
- Does the escalation clock create greed?
- Is cargo theft readable immediately?
- Is recovering a box satisfying?
- Does the HUD feel powerful rather than noisy?
