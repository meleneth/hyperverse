# Design and Architecture Decisions

## D001: Sector Size

Sectors are approximately 9 screens by 9 screens.

## D002: Sector Boundaries

Sectors wrap at all edges.

No immediate lore explanation is required.

## D003: Camera

The ship is framed near horizontal center and approximately three-quarters down the screen.

Position and rotation use tunable lag and dead zones.

There is no absolute north-up requirement.

## D004: Mission Length

Standard missions target approximately 20 minutes.

Some contracts target approximately 5 minutes.

## D005: Escalation

Sector pressure increases on a tunable interval. The design target is a 5-minute interval, but development builds currently run it at 1 minute so escalation can be observed without long sessions.

The HUD announces escalation loudly.

## D006: Quota

Quota is contract-defined.

Possible quota dimensions include mass, value, named minerals, specific recovered items, or combinations.

Meeting quota authorizes extraction.

Staying after quota increases bonuses.

## D007: Extraction Site

Mined cargo accumulates at a remote extraction point.

Queued cargo is generally safe during the mining phase because the site is difficult for raiders to locate.

## D008: Escort Transition

Extraction activates a connected cargo train.

The final phase is an autoscrolling escort toward a gate.

## D009: Cargo Construction

Cargo is stored in connected boxes using relatively weak electromagnetic couplings.

## D010: Cargo Propulsion

A rear propulsion module follows a route plotted and broadcast by the player's ship.

It has propulsion, minimal hardened control circuitry, and no valuable usable power generation.

## D011: Cargo Sensors

Cargo boxes have minimal sensors.

Radiation-hardened sensing and navigation hardware is expensive and therefore not duplicated across dumb boxes.

## D012: Raider Motivation

Raiders seek valuable cargo rather than player destruction for its own sake.

## D013: Raider Theft

A raider approaches the cargo train, disrupts a coupling in approximately 0.5 seconds, and tows one box.

A cargo box occupies the ship's single tow slot.

## D014: Escape Condition

A raider escapes with a stolen box when wrapped shortest-path distance from the player reaches 4.5 screen-lengths.

## D015: Recovery

If the player destroys the raider before escape, the box reacquires the player's beacon, returns to the plotted route, and reconnects.

## D016: Tow Physics

Towing increases mass and therefore reduces acceleration, braking, and maneuver response.

It does not impose arbitrary drag or necessarily reduce theoretical top speed.

Aggressive maneuvers increase tow stress.

Exceeding the tow envelope breaks the coupling.

## D017: Cargo Durability

Standard cargo boxes are sturdy and generally survive tow breaks.

Special cargo may introduce instability, degradation, radiation, or other consequences.

## D018: Drones

Drones are mostly autonomous.

Players choose loadouts and mark priorities.

An endgame capacity around 8 drones is a working possibility, not a fixed commitment.

## D019: Control Architecture

Physical controls map into semantic intent through active control contexts and state machines.

Gameplay systems should not directly depend on raw stick identity.

## D020: Technology

- C++23
- CMake
- CPM
- SDL3
- Dawn/WebGPU
- Jolt
- EnTT
- EventPP
- Boost.Ext SML
- Catch2

Linux and Steam Deck are primary targets.

MSYS2 may be used for initial scaffolding.

## D021: Placeholder Art

Initial sprites come from `meleneth/sector7`, branch `master`.

## D022: Asteroid Breakup

Asteroids start large and can break into smaller child asteroids.

The impact source determines fragment motion. Laser breakup stays coherent, kinetic impacts transfer projectile velocity, and explosive impacts scatter fragments radially. Glancing kinetic impacts also impart angular velocity based on impact offset and projectile velocity.

Large asteroids should support chemical scanning. Composition should influence breakup output with tunable distributions and deliberate loss, not exact hard-coded equal partitions.

Current breakup creates recoverable component chunks rather than cloning the parent composition onto every child.

## D023: Harpoon Tether

The harpoon latches to locked asteroids and uses the ship engine as the asteroid slowdown force.

It is not a magic brake. The ship is pulled toward the asteroid surface velocity, including spin, so large spinners are dangerous. Burst speed detaches the harpoon immediately.

Harpoon influence on the asteroid is mass-limited. Large rocks can be wrangled over time, but they should not stop as if the ship were all-powerful.

## D024: Threat Terminal State

Mining degrades local space. If the player stays alive indefinitely, pressure eventually opens a terminal space tear that consumes the ship.

This gives the round an absolute endpoint and keeps optimal play from becoming "survive forever."

## D025: Raider Tasks

Raiders carry an explicit current task separate from coarse movement phase.

Initial tasks are cargo theft, player harassment, cover for an active thief, and full aggression. This gives raider groups room to coordinate without encoding all behavior in one theft path.

## D026: Weapon Firing Architecture

Particle beam firing should be event-driven and modeled as a small FSM.

The particle beam is a dual-fire weapon with two side-by-side shots. Each shot has independent collision and impact handling.

The FSM should consume semantic fire intent and simulation-clock events, then emit projectile spawn events.

## D027: Universe Clock

The canonical simulation clock is 60 Hz.

`UniverseClock::FixedTickSeconds` is the shared base tick for fixed simulation, weapon FSMs, cooldowns, AI, escalation, and HUD animation. Rendering may interpolate between fixed simulation ticks.
