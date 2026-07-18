# Glossary

`AccountCtx`
: Account-level authority wrapper for registry, RNG, logging, account state, physics, and domain events.

`DomainEvent`
: A transient gameplay fact carried by `DomainEventBus`.

`EntityCtx`
: Per-entity gameplay authority for systems that act on one entity during a sector tick.

`Gravity Sling`
: Player mechanic that constrains radial distance to an eligible target while allowing thrust to shape release velocity.

`SectorTickCtx`
: Fixed-timestep gameplay context with sector tuning and `dt`.

`SML`
: Boost.Ext SML, used for small explicit transition tables.
