# TODO

## Foundations

- Replace the current maxed-out ship computer defaults with progression. Early equipment should degrade HUD accuracy/detail, scan resolution, prediction quality, and warning clarity; upgrades should restore the maxed-out HUD currently used for development.
- Keep sprite collision data generated from alpha masks. Re-run `scripts/generate-sprite-collision-shapes.py` whenever collision-relevant sprites change.
- Add the dynamic camera zoom model and feed its current view scale into `CollisionProbe::zoom_scale`; swept collision budgeting is ready for speed/view scaling, but the camera still needs to own the actual zoom policy.
- Continue shrinking `AppRuntime` toward platform loop responsibilities by moving gameplay lifecycle reactions behind typed contexts and event responders.
- Convert remaining hand-managed phase/state transitions into small SML machines or explicit transition owners:
  - asteroid and enemy target lock lifecycle
  - engine source glow lifecycle if visual issues continue

## Weapons

- Split player weapon command handling further from direct app-loop orchestration. `ParticleCannonModel` already owns ready/cooldown state and dual-shot spawning; new weapons should follow that event-visible command pattern.
- Add authored gameplay tools for the impact kinds that already exist in fragmentation:
  - laser: fragments continue on nearly the same vector
  - kinetic missile: fragments receive direct imparted velocity from the projectile
  - explosive missile: fragments scatter radially

## Asteroids

- Add deep scan output for large asteroids that exposes chemical composition before breakup.
- Use scanned composition to drive breakup results. Example target behavior: an asteroid with four roughly equal mineral groups can split into about three recoverable child chunks, with one portion destroyed or lost during fracture.
- Replace exact mineral-count assumptions with tunable distributions. Breakup should preserve the concept of mass/composition conservation with deliberate loss, not hard-code exact N-way splits.

## Raiders and Cargo

- Keep cargo box lifecycle changes routed through `transition_cargo_box`; new train, drone, extraction, raider, and recovery behavior should not write `CargoBox::state` directly.
- Keep raider phase changes routed through `transition_raider_phase` and task changes through `transition_raider_task`; new combat/theft behavior should emit the matching raider events from those FSM owners.
- Audit combat raider behavior for more task-specific SML boundaries as aggression, cover, retreat, and weapon posture become more complex. Movement can remain procedural until the transition surface grows.
