# AGENTS.md

## Purpose

This repository contains Hyperverse, a C++23 game built around gamepad feel, an aggressively informative interactive HUD, asteroid mining, autonomous drones, escalating sector pressure, and cargo escort.

Codex should implement the game as a sequence of small playable vertical slices. Do not build speculative frameworks ahead of the current slice.

## Technical Baseline

- C++23
- CMake
- CPM
- SDL3 for platform, windowing, input, and gamepad support
- Vulkan for rendering
- Jolt for physics
- EnTT for entity and component storage
- EventPP for event distribution
- Boost.Ext SML for explicit state machines
- Catch2 for tests
- Linux and Steam Deck are primary targets
- MSYS2 is an acceptable initial bootstrap environment

## Working Rules

### 1. Build the smallest playable thing

Each milestone must end with something that can be launched, controlled, observed, and evaluated.

Avoid creating broad subsystem scaffolding without an immediate playable consumer.

### 2. State lives in state holders

- EnTT components hold entity state.
- Explicit subsystem models hold non-entity state.
- SML machines hold transition logic and mode state.
- EventPP carries facts and notifications.

Do not treat the event bus as persistent state.

### 3. Controls are semantic

Gameplay code must not ask for raw left-stick or right-stick values.

Input processing should produce semantic intent such as:

- desired movement
- primary aim
- secondary aim
- tool intensity
- confirm
- cancel
- target cycle
- mark priority
- mode request

The active control context or state machine decides how device input maps to intent.

### 4. Prefer small state machines

Do not create one global game state machine.

Expected separate machines include:

- mission phase
- ship operational posture
- target lock
- mining operation
- cargo escort state
- HUD focus and interaction
- drone task state
- raider theft state

Machines should communicate through explicit events and shared domain state.

### 5. Fixed simulation

Use a fixed simulation timestep. Rendering may interpolate.

Input snapshots should be sampled and converted into semantic intent once per simulation tick.

### 6. Test behavior, not library trivia

Catch2 tests should cover:

- state transitions
- semantic input mapping
- wrapped-sector distance calculations
- cargo tow break conditions
- quota and bonus progression
- raider cargo theft lifecycle
- drone task selection
- deterministic calculations that drive HUD warnings

Do not write tests whose only purpose is proving that EnTT, Jolt, SDL3, or SML work.

### 7. Keep the HUD data-driven

HUD widgets should consume stable read models or presentation snapshots.

Do not let rendering code become the owner of gameplay decisions.

All important hidden calculations should be available to HUD presentation, including:

- collision prediction
- wrapped distance
- tow stress
- escape progress
- quota status
- payout modifiers
- scan confidence
- hazard state
- drone intent
- current control mapping

### 8. Make tuning cheap

Camera lag, acceleration, braking, rotational assistance, tow limits, escalation timing, lock behavior, warning thresholds, and HUD animation timings must be data-configurable.

Avoid hard-coding feel constants deep inside implementation logic.

### 9. Keep commits intentional

Each commit should represent one coherent behavior or infrastructure step.

Good examples:

- bootstrap SDL3 Vulkan window
- add fixed-timestep game loop
- add semantic gamepad intent
- add wraparound sector transform
- add lagged camera anchor
- add asteroid target lock state
- add quota authorization HUD
- add cargo tow stress model

### 10. Definition of done

A feature is done when:

- it builds on supported development platforms
- relevant tests pass
- warnings are clean
- gameplay behavior is observable
- the HUD exposes required state
- tunable values are not buried
- documentation is updated when behavior or architecture changes

## Non-Goals for Initial Work

Do not begin with:

- a complete market simulation
- eight-drone endgame orchestration
- procedural galaxy generation
- final asset pipelines
- multiplayer
- realistic orbital mechanics
- complex ship construction
- full faction simulation
- a giant generalized UI framework

The first goal is a ship that feels good moving through a wraparound sector with a useful HUD.
