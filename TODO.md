# TODO

## Foundations

- Pick and document the base universe clock. The current simulation uses a fixed timestep, but the design still needs an explicit canonical simulation tick duration and policy for scheduling gameplay events.
- Convert particle cannon firing into an event-driven FSM. Firing cadence, cooldown, dual-barrel alternation/synchronization, and projectile spawn events should be transition-driven rather than ad hoc button handling.
- Replace the current maxed-out ship computer defaults with progression. Early equipment should degrade HUD accuracy/detail, scan resolution, prediction quality, and warning clarity; upgrades should restore the maxed-out HUD currently used for development.
- Keep sprite collision data generated from alpha masks. Re-run `scripts/generate-sprite-collision-shapes.py` whenever collision-relevant sprites change.

## Weapons

- Make the particle beam a dual-fire weapon: two side-by-side shots spawned from separate muzzle offsets, each with its own collision query and impact event.
- Split weapon impact kinds explicitly:
  - laser: fragments continue on nearly the same vector
  - kinetic missile: fragments receive direct imparted velocity from the projectile
  - explosive missile: fragments scatter radially

## Asteroids

- Add deep scan output for large asteroids that exposes chemical composition before breakup.
- Use scanned composition to drive breakup results. Example target behavior: an asteroid with four roughly equal mineral groups can split into about three recoverable child chunks, with one portion destroyed or lost during fracture.
- Replace exact mineral-count assumptions with tunable distributions. Breakup should preserve the concept of mass/composition conservation with deliberate loss, not hard-code exact N-way splits.
