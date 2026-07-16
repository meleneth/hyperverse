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
- Activate cargo escort after quota authorization: `Space` or south face button
- Cancel/quit: `Escape` or east face button

The current Vulkan prototype draws Sector7-derived sprites, hardware-uploaded textures, line HUD
brackets, mining beams, cargo boxes, active cargo train links, an escort gate route, and an active
escort raider. The window title is the active HUD: it reports position, speed, camera anchor, edge
warning, target distance, scan confidence, closing speed, rock integrity, heat, extracted ore, cargo
quota, sector pressure, drone state, collision warnings, escort train state, gate distance, and
raider disruption state.

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
