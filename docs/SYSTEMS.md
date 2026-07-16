# Gameplay Systems

## Flight

The ship uses strong flight assistance.

The control fantasy is desired motion rather than raw thruster micromanagement.

The flight model should preserve inertia and mass while allowing the player to feel precise and competent.

Primary tuning dimensions:

- acceleration
- braking
- rotational response
- assisted facing
- lateral correction
- camera position lag
- camera rotation lag
- target-relative assistance
- transition smoothing between operational modes

## Operational Modes

The ship may transition among modes including:

- normal assisted flight
- target approach
- asteroid lock
- mining posture
- mobile weapons platform
- HUD command state
- cargo escort
- towing

The same physical controls may mean different things in different modes.

Example:

### Normal flight

- left stick: desired movement
- right stick: primary aim

### Mobile weapons platform

- left stick: first firing direction
- right stick: second firing direction
- movement: handled by assistance, target-relative behavior, or another context-specific control channel

Exact mappings remain subject to playtesting.

## Asteroid Lock

Locking onto an asteroid is a navigational affordance.

Once lock is established, other systems may become available:

- deep scan
- structural mapping
- ore analysis
- volatile gas detection
- threat assessment
- tool solution
- drone priority marking

Locking is not itself mining.

## Mining

Mining is a family of interacting systems rather than one fixed minigame.

Possible dials include:

- heat
- pressure
- structural fracture
- cutting order
- impact angle
- tool choice
- mineral response
- gas pockets
- radiation
- unstable debris
- controlled detonation

Different asteroid families should use different combinations.

Long-term mastery should allow hazards to become value:

- vent gas
- capture gas
- freeze material
- trigger controlled explosions
- redirect fragments
- expose deeper seams
- harvest irradiated or unstable material safely

Asteroids should begin as large bodies and break into smaller child bodies under tool pressure.

Breakup is not one generic explosion. The impact source determines the fragment velocity field:

- laser: child pieces keep nearly the same vector, with only small divergence
- kinetic missile: child pieces inherit direct imparted velocity from the projectile
- explosive missile: child pieces scatter outward in many directions

Large asteroids should be scannable before breakup. Scan output should expose chemical makeup clearly enough for the player to decide whether to mine, fracture, or destroy the body.

Composition should influence child chunks. For example, an asteroid with four roughly equal mineral groups might produce about three recoverable chunks while one portion is destroyed, vaporized, or dispersed. The implementation should use tunable N-way distributions rather than exact hard-coded ratios.

## Weapons

Weapon firing should be event-driven.

The particle beam is intended as a dual-fire weapon: two side-by-side shots from separate muzzle offsets. Each shot must be an independent projectile with its own collision query and impact event.

The firing mechanism should be modeled as a small FSM driven by semantic fire intent and simulation-clock events. Cooldown, burst timing, dual-barrel synchronization, and projectile spawn events belong in that machine rather than in direct input polling.

## Time

The universe needs one canonical simulation clock.

Current code uses fixed timestep simulation, but the project still needs an explicit base tick decision that gameplay schedulers, weapon FSMs, AI, escalation, cooldowns, and HUD timings can share.

## Ship Status and Computers

The ship has two survivability pools:

- shields regenerate over time
- armor does not regenerate and requires repair

The HUD is not assumed to be perfectly effective forever. Ship computer upgrades should determine HUD effectiveness, scan resolution, prediction quality, warning clarity, and how much hidden calculation is exposed. Development starts with maxed-out computer capability so the full information model can be built and tuned first; game flow should later start the player with weak equipment that improves through farming and upgrades.

## Collision Shapes

Sprite collision shapes are generated from alpha masks rather than hand-authored boxes.

`scripts/generate-sprite-collision-shapes.py` reads transparent areas from sprite PNGs and emits checked-in C++ data. Runtime Jolt queries build compound shapes from that generated data. This keeps collision reviewable and avoids decoding image files during collision setup.

## Drones

Drones are mostly autonomous.

The player chooses loadouts, marks priorities, and influences high-level behavior.

Candidate drone roles:

- mining
- collection
- combat
- interception
- shielding
- repair
- scanning
- venting
- towing
- relay fire

Endgame may support approximately 8 drones, but this is a tuning target rather than a fixed promise.

Initial implementation should begin with 2 drones:

- one mining drone
- one combat drone

## Cargo Train

Cargo consists of connected boxes held together by relatively weak electromagnetic couplings.

The rear propulsion module is cheap, rugged propulsion hardware.

It contains:

- sealed propellant
- simple thrusters
- minimal hardened controls
- a small battery for control and coupling systems
- a beacon follower

It does not contain:

- a valuable reactor
- useful power generation
- expensive independent navigation
- sophisticated sensors

The boxes themselves have minimal sensors because radiation-hard sensing hardware is expensive.

## Tow System

A ship has one tow slot.

A detached cargo box occupies that slot.

Towing does not impose fictional space drag. It increases total mass and therefore affects:

- acceleration
- braking
- directional changes
- rotational response
- coupling stress

The box is stabilized during ordinary motion.

Aggressive evasive maneuvers cause it to lag, swing, and build stress.

If tow angle, relative velocity, or force exceeds the tow envelope, the coupling breaks and the box continues on its current trajectory.

Standard cargo boxes are durable and usually survive a tow break.

Some cargo types may have special consequences:

- volatile material may become unstable
- delicate material may lose value
- radioactive material may create exposure
- cryogenic cargo may degrade

## Raider Theft

A raider theft sequence:

1. approach the cargo train
2. reach a target box
3. disrupt its electromagnetic coupling in approximately 0.5 seconds
4. attach the box to the raider's tow slot
5. flee
6. increase wrapped distance from the player
7. escape once the raider reaches 4.5 screen-lengths from the player

Distance is computed using shortest-path wrapped-sector distance.

If the player destroys the raider before escape:

- the cargo box reacquires the player's beacon
- it follows the plotted course
- it returns to the train
- it reconnects when able

## Escalation

Pressure increases on a tunable interval. The design target is a 5-minute beat, while current development builds use a 1-minute beat so escalation can be evaluated quickly.

The HUD should announce escalation aggressively.

Escalation may affect:

- raider count
- raider capability
- hazard frequency
- asteroid instability
- environmental radiation
- projectile density
- market bonus
- contract multiplier
- extraction danger

The player should understand that staying longer is both more valuable and more dangerous.
