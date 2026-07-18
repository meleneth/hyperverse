# EnTT

Entity state lives in the EnTT registry.

Common component examples:

- `ShipMotion`
- `AsteroidBody`
- `MiningDrone`
- `RaiderShip`
- `CargoBox`
- `ParticleCannonModel`

Events that carry entity handles must not assume the entity is still valid when consumed. Check `registry.valid(entity)` and required components before use.
