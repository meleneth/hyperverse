# Event Reference

All events use `DomainEvent` from `include/hyperverse/domain_events.hpp`.

| Field | Meaning |
| --- | --- |
| `type` | The `DomainEventType` being emitted. |
| `subject` | Primary entity, usually the thing acting or being created. |
| `target` | Secondary entity, usually the affected object. |
| `position` | World-space position associated with the event. |
| `amount` | Event-specific scalar value such as damage or mass. |
| `count` | Event-specific integer count. |

Entity fields may be `entt::null`. Consumers must revalidate entity handles before reading components.

## Events

| Event | Producer | Consumer | Payload |
| --- | --- | --- | --- |
| `ParticleFired` | Particle cannon spawning | HUD/effects consumers | `subject` projectile, `position` muzzle position |
| `ParticleImpact` | Projectile simulation | HUD notice and impact feedback | `subject` projectile, `target` hit entity, `position` impact |
| `HomingMissileFired` | Homing missile launcher | HUD/effects consumers | `subject` missile, `target` locked hostile, `position` launch point |
| `HomingMissileIgnited` | Homing missile flight FSM | HUD/effects consumers | `subject` missile, `target` locked hostile, `position` ignition point |
| `HomingMissileImpact` | Homing missile simulation | HUD/effects consumers | `subject` missile, `target` raider, `position` impact, `amount` damage |
| `AsteroidDamaged` | Projectile damage application | Mining/HUD feedback | `target` asteroid, `position` impact, `amount` damage |
| `AsteroidFragmented` | Asteroid fragmentation | Effects/HUD consumers | `subject` source asteroid, `count` pieces |
| `AsteroidConsumed` | Asteroid depletion | Mining progression consumers | `subject` asteroid |
| `DroneTargetReleased` | Mining drone update | HUD/target feedback | `subject` drone when known, `target` asteroid |
| `ContractAccepted` | Session control | `GameSessionModel` handlers | No entity payload |
| `CargoEscortStarted` | Cargo escort state | Route/encounter consumers | No entity payload |
| `CargoArrivedAtGate` | Cargo escort route arrival | `AppRuntime` raider spawning and notice | `position` gate position |
| `CargoBoxCreated` | Cargo box creation | Cargo dispatch queue | `subject` cargo box, `position` spawn |
| `CargoBoxStateChanged` | Cargo box lifecycle FSM | HUD/debug/gameplay consumers | `subject` cargo box, `position` box position, `amount` mass, `count` new `CargoBoxState` |
| `CargoDroneJobQueued` | Cargo dispatch | HUD/debug consumers | `subject` cargo box, `position` destination |
| `CargoDroneJobAssigned` | Cargo dispatch | HUD/debug consumers | `subject` drone, `target` cargo box, `position` destination |
| `CargoBoxPickupStarted` | Drone cargo FSM | HUD/debug consumers | `subject` drone when known, `target` cargo box |
| `CargoBoxDeliveredToGathering` | Drone cargo FSM | Cargo dispatch queue | `subject` drone when known, `target` cargo box |
| `CargoBoxExtracted` | Cargo extraction | Cargo/HUD consumers | `subject` cargo box |
| `CargoExtractionComplete` | Cargo extraction | Session/progression consumers | `count` extracted cargo count |
| `ContractRoundCompleted` | Cargo extraction or session helper | `GameSessionModel` handlers | No entity payload |
| `MiningTargetCycleRequested` | Radar control FSM | Targeting/HUD consumers | No entity payload |
| `EnemyTargetCycleRequested` | Radar control FSM | Targeting/HUD consumers | No entity payload |
| `RadarTargetsCleared` | Radar control FSM | Targeting/HUD consumers | No entity payload |
| `RaiderPhaseChanged` | Raider phase FSM | HUD/debug/gameplay consumers | `subject` raider, `target` cargo box when relevant, `position` raider position, `amount` integrity, `count` new `RaiderPhase` |
| `RaiderTaskChanged` | Raider task owner | HUD/debug/gameplay consumers | `subject` raider, `target` cargo box when relevant, `position` raider position, `amount` integrity, `count` new `RaiderTask` |
| `GravitySlingPhaseChanged` | Gravity Sling FSM | HUD/debug/gameplay consumers | `subject` ship, `target` sling target when relevant, `amount` `GravitySlingDisengageReason`, `count` new `GravitySlingPhase` |

## Adding an Event

1. Add a value to `DomainEventType`.
2. Document the intended producer, consumer, and payload here.
3. Emit the event near the gameplay fact being created.
4. Install listeners at a composition boundary or through a clearly owned subsystem installer.
5. Add tests for any state transition caused by the event.
