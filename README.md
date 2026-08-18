# Hyperverse

Hyperverse is a gamepad-first, HUD-forward, elevated Asteroids-style mining and escort game.

The player pilots a highly assisted operational mining spacecraft through compact wraparound sectors, identifies and exploits valuable asteroids, builds a modular cargo train, and then escorts that cargo through escalating raider pressure toward a gate.

The game is about feel, information, and controlled industrial violence.

## Current Project Shape

- Playable 2D Vulkan prototype in continuous wraparound sectors
- Each sector is fixed at 9 by 9 1920x1080 reference screens, independent of the current window or browser resolution
- Fixed 60 Hz simulation clock through `UniverseClock::FixedTickSeconds`
- Development round pressure currently escalates every 1 minute
- The long-form design target remains longer contracts with larger escalation beats
- Quota completion authorizes cargo escort and extraction
- Remaining past quota increases payout
- The final phase becomes a cargo escort to a remote jump gate
- Linux and Steam Deck are primary targets
- Initial development may begin under MSYS2
- C++23
- CMake
- CPM
- SDL3
- Vulkan 1.2
- Jolt
- EnTT
- EventPP
- Boost.Ext SML
- Catch2

## Bootstrap

The repository builds a `hyperverse` executable and a Catch2 test target. The current executable
opens an SDL3 Vulkan window, initializes gamepad support, renders sprite assets and HUD geometry,
and runs the playable mining/escort prototype.

See [Installation and Bootstrap](docs/INSTALL.md) for Linux, Steam Deck, MSYS2, CI, and install
commands.

Build and test the deployable Steam Deck bundle in Valve's pinned Steam Linux Runtime SDK with
`./scripts/build-steam-runtime.sh`; copy it to a Deck over SSH with
`./scripts/deploy-steam-deck.sh deck@HOST`.

### Tracy profiling

Profiling is optional and disabled in normal builds. Configure a dedicated instrumented build with:

```sh
cmake -S . -B build-tracy -DHYPERVERSE_ENABLE_TRACY=ON -DHYPERVERSE_BUILD_TESTS=OFF
cmake --build build-tracy -j2
```

Start the Tracy 0.13.1 profiler, connect to the running `build-tracy/hyperverse` client, and capture.
The initial timeline exposes complete frames, fixed simulation ticks, SDL event pumping, render-frame
construction/submission, Vulkan draw/present, drone navigation/contact constraints, raider AI, and
swept asteroid collision. On-demand mode keeps capture traffic inactive until the profiler connects.

## Documentation

The embedded project guide is published through GitHub Pages at
`https://meleneth.github.io/hyperverse/`. The source lives in [docs/tour](docs/tour),
uses VitePress, and is deployed by [.github/workflows/deploy.yml](.github/workflows/deploy.yml)
on pushes to `main`.

Run the docs site locally with:

```sh
cd docs/tour
npm ci
npm run dev
```

Build the same artifact that Pages publishes with:

```sh
cd docs/tour
npm run build
```

Architecture docs should be updated with gameplay changes, especially [events](docs/tour/event-reference.md),
[context objects](docs/tour/context-objects.md), and
[state machines](docs/tour/state-machines.md). Local generated API docs can be built with
`./make_docs.sh` when Doxygen is installed; output is written to `docs/html`.

## Architecture Snapshot

`GrandCentral` is the composition root and owns account-wide runtime state: the EnTT registry,
random-number generator, scoped logger, account state, physics world, and EventPP domain bus.
Startup code derives an `AccountCtx` from it, and gameplay code receives only narrower typed
contexts such as `SectorTickCtx`, `EntityCtx`, `WeaponCtx`, and `ProjectileSimCtx`.

`AppRuntime` is an internal application-loop object built from `AccountCtx`. It handles SDL input,
fixed-timestep advancement, event processing, and renderer-neutral frame submission. It is not a
service locator and should not be passed into gameplay systems.

## Visual Bootstrap

Initial placeholder art should reuse sprites from:

`https://github.com/meleneth/sector7`

Repository default branch: `master`.

The current local copy lives in `assets/sector7/sprites` with provenance notes in
`assets/sector7/README.md`.

These assets are temporary implementation scaffolding, not a permanent visual constraint.

## Current Controls

- Move: `WASD` or left stick
- Aim/facing assist: arrow keys or right stick
- Cycle/lock asteroid mining target: `Tab` or right shoulder
- Cycle/lock enemy target: `\` or left shoulder
- Clear mining and enemy targets: left shoulder + right shoulder
- Fire mining laser at locked asteroid: `F` or right trigger
- Fire particle cannon: `E` or west face button
- Fire homing missiles: `R` or left trigger, while an enemy target is locked
- Burst of speed: east face button
- Engage/release Gravity Sling: `Q` or north face button
- Activate cargo escort after quota authorization: `Space` or south face button
- Cancel/unlock targets: `Escape`
- Quit from keyboard: `Ctrl+Escape`

## Current Playable State

The current Vulkan prototype draws Sector7-derived sprites, hardware-uploaded textures, line HUD
brackets, text glyph HUD, mining beams, dual particle cannon shots, cargo boxes, active cargo train
links, Gravity Sling constraint feedback, an escort gate route, drones, and raiders.

The ship uses assisted desired-motion flight, a short high-thrust boost, shields, armor, semantic
input mapping, and a fixed-step simulation loop. Continued thrust can build velocity without an
artificial top speed. Boost detaches cargo tow links.

Asteroids are large moving bodies with explicit mass, structural break progress, two break levels,
composition, ore rarity, velocity, and spin. Kinetic particle shots apply linear impulse, and
glancing hits apply angular impulse. Breakup produces recoverable component chunks instead of
cloned copies of the parent composition, with some material lost when the parent has several
meaningful component groups.

The Gravity Sling acquires a nearby eligible asteroid and constrains the ship to a stable radius
around that body. While active, normal thrust is redirected into relative orbital motion so the
player can bleed speed, build a launch angle, and release into free flight from the resulting
world-space velocity.

Ship and raider movement now use a stricter spacecraft model: thrust changes velocity, micro
rotation changes facing, braking is counterthrust, and boost increases thrust authority rather than
setting speed directly. Swept gameplay collision checks prevent fast ships from tunneling through
asteroids; contact destroys players, drones, and raiders and produces an explosion.

Mining drones operate autonomously against valid target sizes, spread around work targets, return
to formation, haul pending cargo from the mining site, and break off when their target is invalid.
Destroyed drones respawn after 15 seconds. The current prototype still starts with eight strong
drones so high-end behavior is visible before progression is designed.
Drone formation, mining, and cargo routes use acceleration-limited thrust with predictive asteroid
avoidance and neighbor separation; destroyed drones clear their engine trails immediately.

Cargo is generated from extracted mass and ore value, inherits ore color, follows the ship as a
train during escort, stages as a group near the jump gate, and extracts sequentially. Raiders can
attack cargo and the player; gate arrival can spawn combat raiders. Raider AI now tracks explicit
tasks including cargo theft, drone suppression, player harassment, covering an active thief, and
full aggression. Combat raiders prioritize live drones, fly acceleration-limited swooping passes,
and steer away from predicted asteroid impacts.
Threat escalation now spawns additional combat raider contacts around the player; high pressure
contacts enter full aggression.

The HUD reports position, speed, ship health, round timer, threat level, mining target state, enemy target state, target mass,
ore rarity value, mineral composition, extracted ore, cargo quota, sector pressure, drone state,
collision warnings, escort state, raider disruption, stolen cargo escape, recovery state, and
Gravity Sling state. The upper-right HUD face-button legend shows current face-button meanings and
changes when the tool trigger is held. The top-center urgency HUD shows round time remaining,
current threat level, next threat countdown, and progress toward the next escalation.

## Current Gameplay Considerations

- Ore value is tiered and color-coded by rarity. Cheap bulk ore is intentionally much less important than premium rare, exotic, and anomalous material.
- Asteroid mass and structural damage are separate. Damage means "how close this rock is to breaking up"; extraction means "how much useful mass has been removed."
- Round timer and threat escalation are primary pressure systems. Threat level advances on the development one-minute cadence, spawns raider contacts, and is visible as a countdown/progress bar. Mining eventually destabilizes local space enough to open a terminal space tear.
- Large asteroids have two break levels. Breakup creates recoverable component chunks from the parent mineral distribution, with some material deliberately lost when the parent has several meaningful components.
- Kinetic particle impacts transfer velocity into asteroid mass. Glancing kinetic hits also impart rotational velocity from impact angle, so spinning rocks become a real hazard.
- The Gravity Sling turns eligible asteroids into temporary moving terrain. The ship maintains a constrained radius around the target, uses thrust to change relative orbital motion, and releases with inherited target translation plus player-shaped orbital velocity.
- Mining drones cannot work the largest asteroid tier. The player must break big rocks into medium or small pieces before drones can safely mine them.
- Mined cargo starts at the gathering site. The extraction gate is derived as the furthest wrapped-sector point from that gathering site.
- During escort, cargo follows the player as a train. Once the player reaches the extraction gate, cargo remains gate-bound and stages as a group near the gate before loading starts.
- Boost temporarily doubles thrust authority. Bursting while towing cargo breaks the cargo train coupling.
- Gate extraction processes cargo boxes sequentially at roughly five seconds per box. A round is not complete until extraction finishes.
- Reaching the extraction gate spawns combat raiders that suppress live drones before attacking the player.
- Asteroid damage, fragmentation, consumption, particle fire/impact, cargo lifecycle events, and drone target release are event-visible gameplay facts. New behavior should prefer event responders over hidden direct call chains.
- GrandCentral owns the EventPP bus and context objects expose it. App still contains too much orchestration and should keep shrinking toward platform setup plus GrandCentral startup.

## Known Future Directions

- Replace temporary eight-drone development start with progression-aware drone counts, roles, and upgrades.
- Build more explicit modal control FSMs for flight, mining posture, mobile weapons, Gravity Sling, HUD command, and cargo escort.
- Make ship computer quality drive HUD effectiveness: radar count/range/update cadence, scan confidence, prediction quality, warning clarity, and target detail.
- Expand asteroid scanning into real pre-breakup decision making with chemical makeup, fracture maps, volatile hazards, and tool recommendations.
- Add more asteroid families and hazards: heat, gas, radiation, brittle fracture, controlled detonation, debris, and material-specific reactions.
- Add additional projectile/tool behavior: laser-coherent breakup, kinetic velocity transfer, explosive radial fragmentation, and richer Gravity Sling interactions with larger moving structures.
- Expand threat level effects beyond current raider spawns: richer combat intensity, contract modifiers, gate danger, and pre-tear warning behaviors.
- Improve cargo escort state machines and outcomes: detached cargo recovery, tow stress, extraction sequencing, and loss/payment consequences.
- Move more lifecycle behavior behind EventPP responders and typed contexts; keep shrinking `App` toward platform setup plus `GrandCentral` startup.
- Replace placeholder art and temporary tuning with reviewed production assets, data-driven tuning, and richer feedback.

## Core Fantasy

1. Enter a remote mining sector.
2. Locate and lock onto a sufficiently valuable asteroid.
3. Use scan, analysis, threat, and mapping systems to understand it.
4. Orient the ship and deploy tools and mostly autonomous drones.
5. Extract material while managing heat, fracture, volatile gas, debris, and attackers.
6. Deliver mined material to a remote extraction point.
7. Exceed quota for escalating bonuses.
8. Activate the cargo train.
9. Escort the train through an autoscrolling route toward the gate.
10. Prevent raiders from peeling off cargo boxes and escaping with them.
11. Sell the recovered minerals into a market with varying prices.
12. Improve ship, drone, protection, cargo, and mining capabilities.

## Design Pillars

### Gameplay Feel First

Every movement mode, camera response, weapon, tool, warning, impact, and transition must feel deliberate on a gamepad.

### HUD to the Maximum

The HUD is a primary gameplay surface. It should provide excessive but legible tactical, mining, navigation, cargo, drone, and combat information.

Combat against a rock deserves the same presentation intensity as combat against a raider.

### Operational Competence

The player flies a sophisticated industrial spacecraft with strong assistance. The fantasy is expertise, not wrestling with bad controls.

### Systems Become Opportunities

Heat, gas, radiation, fracture, debris, and hostile pressure begin as hazards. Mature play should allow the player to turn many of them into additional value.

### Event-Driven Control Context

Controls are semantic and modal. Physical inputs do not permanently map to gameplay actions. State machines determine what the controls mean in the current operational posture.
