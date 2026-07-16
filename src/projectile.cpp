#include "hyperverse/projectile.hpp"

#include "hyperverse/grand_central.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

namespace hyperverse {
namespace {

void ensure_jolt_projectile_ready() {
  static std::once_flag jolt_registration;
  std::call_once(jolt_registration, [] {
    JPH::RegisterDefaultAllocator();
    if (JPH::Factory::sInstance == nullptr) {
      JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();
  });
}

[[nodiscard]] Vec2 facing_direction(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

[[nodiscard]] bool jolt_spheres_overlap(float particle_radius, float asteroid_radius, Vec2 relative_position) {
  const JPH::SphereShape particle_shape{particle_radius};
  const JPH::SphereShape asteroid_shape{asteroid_radius};
  JPH::CollideShapeSettings settings;
  JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
  JPH::CollisionDispatch::sCollideShapeVsShape(
    &particle_shape,
    &asteroid_shape,
    JPH::Vec3::sOne(),
    JPH::Vec3::sOne(),
    JPH::Mat44::sIdentity(),
    JPH::Mat44::sTranslation(JPH::Vec3(relative_position.x, relative_position.y, 0.0F)),
    JPH::SubShapeIDCreator(),
    JPH::SubShapeIDCreator(),
    settings,
    collector
  );
  return collector.HadHit();
}

void apply_projectile_damage(AsteroidBody& asteroid, MiningResource& resource, const ParticleCannonTuning& tuning) {
  resource.integrity = std::max(0.0F, resource.integrity - tuning.damage);
  const float remaining_fraction = std::clamp(resource.integrity / 100.0F, tuning.asteroid_min_radius_fraction, 1.0F);
  asteroid.radius = std::max(8.0F, asteroid.base_radius * remaining_fraction);
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
  ensure_jolt_projectile_ready();
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
      if (jolt_spheres_overlap(tuning.projectile_radius, asteroid.radius, relative_position)) {
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
