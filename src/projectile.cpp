#include "hyperverse/projectile.hpp"

#include "hyperverse/account_context.hpp"
#include "jolt_shape_queries.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace hyperverse {
namespace {

[[nodiscard]] Vec2 facing_direction(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

void apply_projectile_damage(AsteroidBody& asteroid, MiningResource& resource, const ParticleCannonTuning& tuning) {
  resource.integrity = std::max(0.0F, resource.integrity - tuning.damage);
  const float remaining_fraction = std::clamp(resource.integrity / 100.0F, tuning.asteroid_min_radius_fraction, 1.0F);
  asteroid.radius = std::max(MinimumPlayableAsteroidRadius, asteroid.base_radius * remaining_fraction);
}

void fragment_depleted_asteroid(
  entt::registry& registry,
  entt::entity asteroid,
  const ParticleShot& projectile,
  const ParticleCannonTuning& tuning
) {
  if (!registry.valid(asteroid) || !registry.all_of<MiningResource>(asteroid) || registry.get<MiningResource>(asteroid).integrity > 0.0F) {
    return;
  }

  (void)fragment_asteroid(
    registry,
    asteroid,
    AsteroidFragmentationRequest{
      .impact_kind = tuning.impact_kind,
      .impact_position = projectile.position,
      .impact_velocity = projectile.velocity,
      .pieces = 4,
    }
  );
}

}  // namespace

ParticleCannonHudSnapshot update_particle_cannon(
  AccountCtx& ctx,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const ParticleCannonTuning& tuning
) {
  entt::registry& registry = ctx.registry();

  if (input.particle_fire_requested) {
    const Vec2 direction = length(input.primary_aim) > 0.0F ? normalize_or_zero(input.primary_aim) : facing_direction(ship.facing_radians);
    const entt::entity projectile = registry.create();
    registry.emplace<ParticleShot>(
      projectile,
      ParticleShot{
        .position = ship.position + (direction * 46.0F),
        .velocity = (direction * tuning.projectile_speed) + ship.velocity,
      }
    );
    ctx.event_bus().enqueue(
      DomainEventType::ParticleFired,
      DomainEvent{
        .type = DomainEventType::ParticleFired,
        .subject = projectile,
        .position = ship.position,
        .amount = tuning.projectile_speed,
      }
    );
  }

  ParticleCannonHudSnapshot hud{};
  std::vector<entt::entity> expired;
  for (auto [projectile_entity, projectile] : registry.view<ParticleShot>().each()) {
    projectile.ttl_seconds -= dt_seconds;
    projectile.position = wrap_position(projectile.position + (projectile.velocity * dt_seconds), sector);
    bool hit = false;

    for (auto [asteroid_entity, asteroid, resource] : registry.view<AsteroidBody, MiningResource>().each()) {
      if (resource.integrity <= 0.0F) {
        continue;
      }

      const Vec2 relative_position = wrapped_delta(projectile.position, asteroid.position, sector);
      if (jolt_shapes_overlap(
            SpriteCollisionShape::Particle,
            tuning.projectile_radius,
            SpriteCollisionShape::Rock,
            asteroid.radius,
            relative_position
          )) {
        apply_projectile_damage(asteroid, resource, tuning);
        ctx.event_bus().enqueue(
          DomainEventType::ParticleImpact,
          DomainEvent{
            .type = DomainEventType::ParticleImpact,
            .subject = projectile_entity,
            .target = asteroid_entity,
            .position = projectile.position,
            .amount = tuning.damage,
          }
        );
        fragment_depleted_asteroid(registry, asteroid_entity, projectile, tuning);
        hit = true;
        break;
      }
    }

    if (hit || projectile.ttl_seconds <= 0.0F) {
      expired.push_back(projectile_entity);
    } else {
      ++hud.active_particles;
    }
  }

  for (entt::entity projectile : expired) {
    registry.destroy(projectile);
  }

  return hud;
}

}  // namespace hyperverse
