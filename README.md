# Hyperverse

Hyperverse is a gamepad-first, HUD-forward, elevated Asteroids-style mining and escort game.

The player pilots a highly assisted operational mining spacecraft through compact wraparound sectors, identifies and exploits valuable asteroids, builds a modular cargo train, and then escorts that cargo through escalating raider pressure toward a gate.

The game is about feel, information, and controlled industrial violence.

## Current Project Shape

- 2D gameplay in continuous wraparound sectors
- Each sector is approximately 9 camera-widths by 9 camera-heights
- Typical mission length: about 20 minutes
- Short contract variants: about 5 minutes
- Five-minute escalation beats increase pressure and reward
- Quota completion authorizes extraction
- Remaining past quota increases payout
- The final phase becomes an autoscrolling cargo escort
- Linux and Steam Deck are primary targets
- Initial development may begin under MSYS2
- C++23
- CMake
- CPM
- SDL3
- Vulkan
- Jolt
- EnTT
- EventPP
- Boost.Ext SML
- Catch2

## Bootstrap

Milestone 0 builds a `hyperverse` executable that opens an SDL3 Vulkan window, clears the
screen through Vulkan, initializes gamepad support, and builds a Catch2 test target.

See [Installation and Bootstrap](docs/INSTALL.md) for Linux, Steam Deck, MSYS2, CI, and install
commands.

## Visual Bootstrap

Initial placeholder art should reuse sprites from:

`https://github.com/meleneth/sector7`

Repository default branch: `master`.

The current local copy lives in `assets/sector7/sprites` with provenance notes in
`assets/sector7/README.md`.

These assets are temporary implementation scaffolding, not a permanent visual constraint.

## Current Prototype Controls

- Move: `WASD` or left stick
- Aim/facing assist: arrow keys or right stick
- Cycle/lock asteroid target: `Tab` or right shoulder
- Fire mining laser at locked asteroid: `F` or right trigger
- Fire particle cannon: `E` or west face button
- Burst of speed: east face button
- Activate cargo escort after quota authorization: `Space` or south face button
- Cancel/quit: `Escape`

The current Vulkan prototype draws Sector7-derived sprites, hardware-uploaded textures, line HUD
brackets, text glyph HUD, mining beams, particle cannon shots, cargo boxes, active cargo train links,
an escort gate route, and an active escort raider. Ship and asteroid motion are stepped through
Jolt. Asteroids vary in size, velocity, rotation, explicit mass, structural break progress, and
color-coded ore rarity; nearby rocks get electric-blue radar brackets. The prototype starts with
eight mining drones until progression is defined. The HUD reports position, speed, target state,
target mass, ore rarity value, mineral composition, extracted ore, cargo quota, sector pressure,
drone state, collision warnings, escort state, raider disruption, stolen cargo escape, and recovery
state.

## Current Gameplay Considerations

- Ore value is tiered and color-coded by rarity. Cheap bulk ore is intentionally much less important than premium rare, exotic, and anomalous material.
- Asteroid mass and structural damage are separate. Damage means "how close this rock is to breaking up"; extraction means "how much useful mass has been removed."
- Large asteroids have two break levels. The first break creates medium pieces, the second creates small pieces, and further depletion consumes the final fragment.
- Kinetic particle impacts transfer velocity into asteroid mass. Getting in front of a moving rock and firing back into its path is the intended way to slow it before mining.
- Mining drones cannot work the largest asteroid tier. The player must break big rocks into medium or small pieces before drones can safely mine them.
- Mined cargo starts at the gathering site. The extraction gate is derived as the furthest wrapped-sector point from that gathering site.
- During escort, cargo follows the player as a train. Once the player reaches the extraction gate, cargo remains gate-bound and stages as a group near the gate before loading starts.
- Burst of speed temporarily pushes the ship past its normal top speed and decays back down. Bursting while towing cargo breaks the cargo train coupling.
- Gate extraction processes cargo boxes sequentially at roughly five seconds per box. A round is not complete until extraction finishes.
- Reaching the extraction gate spawns combat raiders that prioritize killing the player over stealing cargo.
- Asteroid damage, fragmentation, consumption, particle impacts, and drone target release are event-visible gameplay facts. New behavior should prefer event responders over hidden direct call chains.
- GrandCentral owns the EventPP bus and context objects expose it. App still contains too much orchestration and should keep shrinking toward platform setup plus GrandCentral startup.

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
