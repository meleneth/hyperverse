# State Machines

Hyperverse models gameplay mode explicitly. SML-backed systems keep transition tables private and persist the public enum/model state in components or subsystem models.

```mermaid
flowchart TD
  Public[Public enum phase] --> Replay[Replay phase into SML machine]
  Replay --> Event[Process transition event]
  Event --> Read[Read active SML state]
  Read --> Public
```

## SML-Backed Machines

| Model | Public phase | Transition owner | Purpose |
| --- | --- | --- | --- |
| `GameSessionModel` | `GameSessionPhase` | `src/game_session.cpp` | Moves between contract chooser and active round. |
| `CargoEscortState` | `CargoEscortPhase` | `src/cargo_escort.cpp` | Moves from mining to authorized extraction, escort, extraction, and completion. |
| `MiningDrone` cargo mode | `MiningDronePhase` subset | `src/drone.cpp` | Moves cargo drones through unassigned, pickup, escorting, and delivered. |
| `ParticleCannonModel` | `ParticleCannonPhase` | `src/projectile.cpp` | Moves a cannon between ready and cooldown. |
| `FlightInputMapper` | `ControlMapping` | `src/input.cpp` | Chooses keyboard or gamepad mapping based on the most recent observed input source. |

## Game Session

```mermaid
stateDiagram-v2
  [*] --> ContractChooser
  ContractChooser --> PlayingRound: ContractAccepted
  PlayingRound --> ContractChooser: ContractRoundCompleted
```

`accept_contract` and `complete_contract_round` enqueue events and update the local model immediately. Installed event handlers keep externally enqueued contract events in sync with `GameSessionModel`.

## Cargo Escort

```mermaid
stateDiagram-v2
  [*] --> Mining
  Mining --> Authorized: quota_authorized
  Authorized --> Mining: quota_lost
  Authorized --> EscortActive: confirm_extraction
  EscortActive --> Extracting: gate_reached
  Extracting --> Complete: extraction_complete
```

`CargoEscortState` emits `CargoEscortStarted` when the player confirms extraction and `CargoArrivedAtGate` when the cargo reaches the gate.

## Drone Cargo Work

```mermaid
stateDiagram-v2
  [*] --> Unassigned
  Unassigned --> PickupCargo: cargo_assigned
  PickupCargo --> EscortingCargo: cargo_picked_up
  EscortingCargo --> Unassigned: cargo_delivered
```

The durable component remains `MiningDrone`. The cargo FSM owns only the cargo hauling subset; mining, travelling, and idle formation behavior are still explicit update logic around that machine.

## Particle Cannon

```mermaid
stateDiagram-v2
  [*] --> Ready
  Ready --> Cooling: trigger_held
  Cooling --> Ready: cooldown_elapsed
```

Player, drone, and raider cannons share the same model. Tuning changes the cooldown interval for each owner.

## Input Mapping

```mermaid
stateDiagram-v2
  [*] --> Keyboard
  Keyboard --> Gamepad: gamepad_mapping_observed
  Gamepad --> Keyboard: keyboard_mapping_observed
```

Raw SDL input is mapped into semantic intent once per frame. Edge-triggered actions such as confirm, boost, Gravity Sling, and particle fire are computed by comparing the current raw frame with the previous raw frame.

## Explicit Enum State Without SML

Some systems currently use explicit phase enums without SML transition tables:

- `GravitySlingModel` uses `GravitySlingPhase` and `GravitySlingDisengageReason`.
- `TargetLockModel` uses `TargetLockPhase`.
- `RaiderShip` uses `RaiderPhase`, `RaiderRole`, and `RaiderTask`.
- `MiningDrone` uses `MiningDronePhase` outside the cargo-hauling subset.

These are still first-class state models. When transitions become complex, add a private SML transition table and keep the public enum/model as the durable state.
