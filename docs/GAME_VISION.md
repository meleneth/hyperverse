# Game Vision

## Elevator Pitch

Hyperverse is an arcade mining and escort game in which a highly assisted spacecraft dismantles dangerous asteroids under escalating pressure, builds a modular cargo train, and then escorts that train through a raider-infested autoscrolling route.

It is an Asteroids descendant with larger spaces, larger rocks, smarter drones, economic stakes, and a HUD that treats every system as worthy of tactical ceremony.

## Session Structure

### Standard Contract

A standard mission targets approximately 20 minutes.

- Early phase: prospect, scan, mine, and deliver cargo to the extraction point
- Every 5 minutes: sector pressure escalates
- Once contract quota is met: extraction becomes authorized
- The player may remain to earn higher bonuses
- When the player commits to extraction: the cargo train activates
- Final phase: autoscrolling escort toward the gate

### Short Contract

Some contracts target approximately 5 minutes.

These may emphasize:

- emergency extraction
- high-risk sampling
- courier escort
- recovery
- survival
- a compact single-rock mining challenge

## Sector Structure

A sector is fixed at 9 by 9 3840x2160 reference screens, independent of the current window or browser resolution.

The sector wraps at all boundaries.

There is no lore obligation to explain why space behaves this way. Space is weird.

The camera follows the player with tunable positional and rotational lag. The desired composition places the ship near the horizontal center and roughly three-quarters down the screen, preserving useful look-ahead.

## Contract Quotas

The contract defines what counts as profitable completion.

Quota types may include:

- raw cargo mass
- projected market value
- a required amount of a named mineral
- recovery of a specific object or sample
- combinations of requirements

Meeting quota authorizes extraction but does not force it.

Staying beyond quota increases bonuses while the five-minute escalation cycle continues to raise pressure.

## Cargo Flow

During mining, extracted material is delivered to a remote extraction point.

Queued cargo is generally safe because the extraction site is too remote and sparse for raiders to locate reliably.

When extraction begins:

1. cargo boxes connect into a modular train
2. the rear propulsion module activates
3. the player's ship plots and broadcasts the route
4. the train follows that plotted course
5. the mission transitions into an autoscrolling escort phase
6. traffic density near the gate makes the convoy discoverable
7. raiders begin attacking the cargo boxes

## Raiders

Raiders are economically motivated.

They are trying to obtain valuable material, not merely destroy the player.

Their behavior should reflect this:

- approach the train
- disrupt a weak electromagnetic cargo coupling
- claim one cargo box using a tow slot
- flee from the player
- escape when sufficiently distant
- abandon or lose the box if destroyed first

A raider can tow one box at a time because the box occupies its tow slot.

The player also has a tow slot and may recover one detached box at a time.

## Tone

The game may be funny, but its world should remain materially coherent.

Industrial systems are built cheaply where possible. Radiation-hard sensors and navigation electronics are valuable. Cargo boxes are durable, simple, and nearly blind. Raiders make rational economic choices inside an irrational universe.
