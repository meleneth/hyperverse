# Gameplay Systems

## Flight

The ship uses strict thrust physics with strong flight-computer assistance layered on top.

The control fantasy is desired motion rather than raw thruster micromanagement, but the physical
model remains thrust and controlled micro-rotation. Velocity changes only through applied thrust,
braking is counterthrust, and boost increases thrust authority instead of setting velocity directly.

The flight model should preserve inertia and mass while allowing the player to feel precise and competent.

Default ship tuning should stay nimble enough for the player to get ahead of moving asteroids, fire kinetic shots back into their path, and reduce their velocity before committing mining drones.

Primary tuning dimensions:

- acceleration
- braking
- rotational response
- speed envelope assist
- assisted facing
- lateral correction
- camera position lag
- camera rotation lag
- target-relative assistance
- transition smoothing between operational modes

`flight_computer_assist` converts semantic movement and aim intent into a `ThrusterCommand`.
`apply_thruster_physics` consumes that command and owns physical integration: thrust, rotation
rate limiting, speed envelope assist, and sector wrapping. This keeps the eventual flight computer
replaceable without loosening the underlying spacecraft model.

Raiders follow the same physical rule: their steering code chooses thrust direction and facing, then
acceleration changes velocity over time. Raider movement should not assign velocity directly except
for deliberate spawn/setup initialization.

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

## Radar Control

Radar target control is owned by `RadarControlModel`, a small SML-backed state machine. Raw input
only reports shoulder/button state. The radar-control FSM consumes that state and emits target
commands/events:

- right shoulder: cycle asteroid mining targets and enter mining focus
- left shoulder: cycle enemy ship targets
- left shoulder + right shoulder: clear all targets

After a clear chord, the FSM remains blocked while either shoulder is still held. This prevents a
sloppy release from being interpreted as a left-only or right-only cycle. Single-shoulder target
cycling becomes live again only after both shoulders have fully released.

The FSM also records `RadarFocus`: none, mining, or combat. Target locks consume the command frame
produced by the FSM; events notify the rest of the game that the radar-control fact occurred.

Asteroid radar and combat radar are separate persisted models. `RadarHudModel` owns asteroid
contacts and asteroid target order; `CombatRadarHudModel` owns hostile ship contacts and enemy
target order. The lock systems consume those owned order lists instead of sharing one mixed list.

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

Asteroids start as large moving bodies with an explicit mass model, separate structural break progress, and two levels of fragmentation.

Lifecycle:

- a seeded asteroid starts with two remaining break levels
- the first depletion/impact breakup creates medium fragments with one remaining break level
- the second breakup creates small mineable fragments with zero remaining break levels
- structural damage moves the current rock toward breakup
- extraction removes remaining mass and creates value
- further depletion of a zero-break fragment consumes it instead of creating more pieces

Kinetic particle impacts apply projectile velocity into the asteroid mass, so firing from ahead of a moving rock can slow it down. Laser, kinetic, and explosive breakup patterns remain separate tuning/behavior paths.

Mining drones are not allowed to mine the largest asteroid tier. The player must first break large rocks into the two smaller size ranges before drones can work them.

Each mining drone working an asteroid increases the rate at which asteroid mass becomes cargo
container mass. When a cargo container is created, it queues a transport job to the gathering site.
Cargo transport has priority over continued mining: a drone that is not already hauling cargo may
drop its current mining target and pick up a pending container.

Ore value is tiered and color-coded. The intended mining decision is not "extract every kilogram"; it is "identify and extract the premium material before spending time on cheap bulk."

Current rarity/value curve:

- Common: low-value grey utility ore
- Industrial: modest-value amber production ore
- Rare: high-value blue specialist ore
- Exotic: premium magenta material
- Anomalous: extreme-value green material

Asteroid lifecycle facts should be event-visible. Current domain events include damage, fragmentation, consumption, particle fire/impact, and drone target release. New behavior should prefer subscribing/responding to those facts over threading direct calls across unrelated systems.

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

Current breakup creates recoverable component chunks from the parent composition. Each child carries a dominant mineral component and a matching ore tier. When a parent has several meaningful component groups, not every group is guaranteed to survive breakup.

Fast-spinning asteroids are a major physical hazard. Default seeded rocks spin visibly, the Gravity Sling lets the ship borrow that rotating frame, and glancing kinetic particle impacts impart angular velocity according to impact offset and projectile velocity.

## Gravity Sling

The Gravity Sling acquires eligible large physical targets such as asteroids.

While active:

- the target remains authoritative for position, rotation, angular velocity, and destruction
- the ship position is defined by a local polar offset in the target's rotating frame
- movement input adjusts the local angular offset and sling radius within configured bounds
- aim and firing remain available
- release velocity is computed from target translation, target angular velocity, and player-controlled relative angular motion

The intended use is to acquire a moving rock, orbit into a useful firing or escape position, then release into a legible slingshot trajectory. The target temporarily becomes moving terrain rather than a body being slowed by the ship.

The sling radius is locked to the wrapped distance at acquisition, not adjusted to a preferred band
after capture. While active, the constraint keeps the ship on that radius, but the player can turn
and thrust independently. Engine thrust transfers acceleration into the asteroid through the sling,
so pointing opposite the asteroid's travel slows it for follow-up mining.

The phase and disengage reason are owned by a small SML-backed transition owner. It emits
`GravitySlingPhaseChanged` for engagement, activation, and disengage transitions.

## Threat Pressure

Mining destabilizes local space.

Threat level advances on the canonical simulation clock. Development builds currently use a one-minute escalation cadence so pressure behavior is observable quickly. The design target can stretch this for longer contracts.

Escalation should affect:

- raider count
- raider task mix
- raider aggression
- gate danger
- contract modifiers
- HUD warning intensity

Current threat escalation spawns combat raiders around the player. Lower threat contacts harass the player; high threat contacts enter full aggression. The spawn count rises with threat level and is capped for the current slice.

If the player stays indefinitely, local space eventually tears open and consumes the ship. This terminal state prevents survival from becoming an infinite optimization problem.

## Raiders

Raiders have a role, a current phase, and a current task. Phase and task changes are owned by
small SML-backed transition owners so changes are event-visible.

Current tasks:

- steal cargo
- harass the player
- cover an active cargo thief
- full aggression

Cargo thieves prioritize cargo pod theft. Combat raiders harass the player by default, switch to cover behavior when a thief is actively disrupting or towing cargo, and switch to full aggression during extraction or terminal gate pressure.

## Weapons

Weapon firing is event-visible and should keep moving toward event responders at gameplay
lifecycle boundaries.

The particle beam is a dual-fire weapon: two side-by-side shots from separate muzzle offsets. Each shot is an independent projectile with its own collision query and impact event.

The firing mechanism is modeled as a small ready/cooldown FSM driven by semantic fire intent and simulation-clock time. Cooldown, owner-specific cadence, dual-shot spawning, and projectile spawn events live in the weapon path rather than raw input polling.

Homing missiles are a separate locked-target weapon. The launcher consumes semantic missile-fire
intent and the current enemy target lock; without a locked hostile target it does not spawn
missiles. A valid launch ejects two missiles from the left and right side of the player ship. Each
missile coasts for half a second, then its SML-owned flight phase transitions from ejected to
ignited and the motor begins steering toward the locked hostile. Missile impacts emit
`HomingMissileImpact`, damage the raider, and spawn a short-lived explosion burst for the HUD.

## Time

The universe needs one canonical simulation clock.

The canonical simulation clock is 60 Hz. `UniverseClock::FixedTickSeconds` is the shared base tick for gameplay schedulers, weapon FSMs, AI, escalation, cooldowns, and HUD timings. Rendering may still interpolate between fixed simulation ticks.

## Ship Status and Computers

The ship has two survivability pools:

- shields regenerate over time
- armor does not regenerate and requires repair

The HUD is not assumed to be perfectly effective forever. Ship computer upgrades should determine HUD effectiveness, scan resolution, prediction quality, warning clarity, and how much hidden calculation is exposed. Development starts with maxed-out computer capability so the full information model can be built and tuned first; game flow should later start the player with weak equipment that improves through farming and upgrades.

## Collision Shapes

Sprite collision shapes are generated from alpha masks rather than hand-authored boxes.

`scripts/generate-sprite-collision-shapes.py` reads transparent areas from sprite PNGs and emits checked-in C++ data. Runtime Jolt queries build compound shapes from that generated data. This keeps collision reviewable and avoids decoding image files during collision setup.

High-thrust flight can cross collision volumes between fixed ticks, so collision prediction uses a
swept path instead of relying only on per-tick overlap. The predictor first builds a
distance-ranked candidate set around the ship. The number of swept checks scales from ship speed
and zoom/view tuning, then each candidate gets a cheap swept-radius line test before the precise
Jolt shape cast. HUD snapshots expose candidate and swept-check counts so prediction cost and
warning quality can be tuned instead of guessed.

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

Drone movement speed, acceleration/formation response, mining efficiency, extraction efficiency, and travel-to-work behavior should become upgradeable equipment axes. Current builds start with strong values so the slice shows the intended high-end feel first.

The current playable build starts with 8 strong mining drones. That is a development fixture for
making high-end drone behavior visible before progression, loadouts, and role counts are designed.

Cargo delivery preempts mining work. Once a cargo box exists, assigning a drone to transport it is
more important than keeping that drone on an asteroid to create additional cargo.

Drone mode is split across small SML-backed owners. The cargo FSM owns pickup and escorting cargo;
the work FSM owns normal idle formation, travelling to work, and mining. Cargo preemption crosses
between those machines through explicit transition calls instead of a single large drone FSM.

## Cargo Train

Cargo consists of connected boxes held together by relatively weak electromagnetic couplings.

Cargo box lifecycle state is owned by the SML-backed cargo box transition owner. Train, drone,
extraction, raider, and recovery code should request named transitions rather than writing
`CargoBox::state` directly.

Cargo extraction queue mode is owned by `CargoExtractionModel`, another small SML-backed model. It
tracks queueing, active-box gate movement, active extraction, and completion while individual cargo
container lifecycle still stays in the cargo box FSM.

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
