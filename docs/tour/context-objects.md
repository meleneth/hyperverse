# Context Objects

Context objects are small, non-owning authority values. They expose exactly the shared state a system is allowed to use.

The common spine is:

```cpp
registry()
rng()
log()
```

Narrower contexts add only what their scope needs.

## AccountCtx

`AccountCtx` is derived from `GrandCentral` by application startup and wraps the account-level services:

- `entt::registry&`
- `std::mt19937&`
- `ScopedLog&`
- `AccountState&`
- `PhysicsWorld&`
- `DomainEventBus&`

It is the broadest gameplay context currently available. Code below the composition root should prefer a narrower context when possible.

## SectorTickCtx

`SectorTickCtx` is derived from `AccountCtx` for fixed-timestep gameplay. It adds:

- `const SectorTuning& sector()`
- `float dt()`
- `EntityCtx entity_context(entt::entity self)`

Use this for systems that operate across a sector tick and do not represent one specific entity as their authority boundary.

## EntityCtx

`EntityCtx` binds a `SectorTickCtx` to one entity.

It exposes:

- `self()`
- shared world accessors from the tick context
- `get<T>()` and `get_const<T>()` for components on `self()`
- `entity_context(other)` to narrow or switch entity authority inside the same tick

Use it for systems where the acting entity matters, such as weapons.

## Specialized Wrapper Contexts

Some systems wrap these contexts to make authority even more explicit:

- `WeaponCtx` wraps `EntityCtx` and exposes the weapon owner, registry, event bus, sector, timestep, particle cannon component, and homing missile launcher component.
- `ProjectileSimCtx` wraps `SectorTickCtx` plus the player entity for projectile simulation that may damage the player.

These wrappers are preferred over passing several loose references when the grouped authority has a clear domain meaning.

## Rules

- Contexts do not own the objects they reference.
- Context lifetimes must remain shorter than the owning runtime state.
- Do not pass `GrandCentral` or `AppRuntime` into gameplay systems.
- Do not use context objects as generic service locators.
- Tests should construct the narrowest context or model needed for the behavior under test.
