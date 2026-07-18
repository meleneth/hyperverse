# Hyperverse Documentation

Hyperverse is a C++23 arcade space game built around fast ship feel, asteroid mining, drone labor, cargo escort, raider pressure, and a HUD that explains what the simulation is doing.

This documentation is the embedded project guide. It is meant to sit next to the code and stay accurate as vertical slices land.

The published site is built from this directory by `.github/workflows/deploy.yml` and served
through GitHub Pages at `https://meleneth.github.io/hyperverse/`.

## Start Here

- [Architecture](./architecture.md) explains the runtime shape and where state is allowed to live.
- [Event Flow](./event-flow.md) explains how domain facts move through EventPP.
- [Event Reference](./event-reference.md) documents every `DomainEventType`.
- [Context Objects](./context-objects.md) documents `AccountCtx`, `SectorTickCtx`, `EntityCtx`, and narrow wrapper contexts.
- [State Machines](./state-machines.md) documents the SML-backed phase models and the enum-driven state models that still need explicit transition ownership.

## Design Rule

Gameplay systems should depend on narrow context objects and explicit state models, not globals. Events carry facts and notifications. Components and subsystem models own persistent state.
