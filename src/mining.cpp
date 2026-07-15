#include "hyperverse/mining.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {
namespace {

struct MiningTarget {
  entt::entity entity{entt::null};
  float distance{0.0F};
};

[[nodiscard]] Vec2 facing_direction(float facing_radians) {
  return {.x = std::cos(facing_radians), .y = std::sin(facing_radians)};
}

void populate_hud_from_resource(MiningHudSnapshot& hud, const MiningResource& resource, const MiningLaserTuning& tuning) {
  hud.target_integrity = resource.integrity;
  hud.target_heat = resource.heat;
  hud.target_structural_stress = resource.structural_stress;
  hud.target_volatile_pressure = resource.volatile_pressure;
  hud.extracted_mass = resource.extracted_mass;
  hud.gas_venting = resource.venting;
  hud.unstable = resource.heat >= tuning.unstable_heat || resource.structural_stress >= tuning.unstable_stress ||
                 resource.volatile_pressure >= tuning.volatile_pressure_limit;
}

[[nodiscard]] MiningTarget raycast_mining_target(
  entt::registry& registry,
  const ShipMotion& ship,
  Vec2 direction,
  const SectorTuning& sector,
  float range
) {
  MiningTarget best{};
  float best_fraction = std::numeric_limits<float>::max();

  const Vec2 normalized_direction = normalize_or_zero(direction);
  if (length(normalized_direction) <= 0.0F) {
    return best;
  }

  const JPH::Vec3 ray_direction{normalized_direction.x * range, normalized_direction.y * range, 0.0F};
  for (auto [entity, asteroid, resource] : registry.view<AsteroidBody, MiningResource>().each()) {
    if (resource.integrity <= 0.0F) {
      continue;
    }

    const Vec2 relative_position = wrapped_delta(ship.position, asteroid.position, sector);
    const JPH::SphereShape asteroid_shape{asteroid.radius};
    JPH::RayCast ray{{-relative_position.x, -relative_position.y, 0.0F}, ray_direction};
    JPH::RayCastResult hit{};
    if (!asteroid_shape.CastRay(ray, JPH::SubShapeIDCreator(), hit)) {
      continue;
    }

    if (hit.mFraction < best_fraction) {
      best_fraction = hit.mFraction;
      best = {.entity = entity, .distance = hit.mFraction * range};
    }
  }

  return best;
}

[[nodiscard]] MiningTarget resolve_mining_target(
  entt::registry& registry,
  const TargetLockModel& target_lock,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const MiningLaserTuning& tuning
) {
  if (
    has_locked_target(target_lock) && registry.valid(target_lock.target) &&
    registry.all_of<AsteroidBody, MiningResource>(target_lock.target)
  ) {
    return {.entity = target_lock.target, .distance = target_lock.wrapped_distance};
  }

  const Vec2 direction = length(input.primary_aim) > 0.0F ? input.primary_aim : facing_direction(ship.facing_radians);
  return raycast_mining_target(registry, ship, direction, sector, tuning.range);
}

}  // namespace

MiningHudSnapshot update_mining_laser(
  entt::registry& registry,
  const TargetLockModel& target_lock,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const MiningLaserTuning& tuning,
  float dt_seconds
) {
  MiningHudSnapshot hud{};
  hud.beam_intensity = std::clamp(input.tool_intensity, 0.0F, 1.0F);
  const MiningTarget target = resolve_mining_target(registry, target_lock, ship, input, sector, tuning);
  if (target.entity == entt::null) {
    return hud;
  }

  const AsteroidBody& asteroid = registry.get<AsteroidBody>(target.entity);
  MiningResource& resource = registry.get<MiningResource>(target.entity);
  hud.target = target.entity;
  hud.target_in_range = target.distance <= tuning.range;
  hud.beam_end_position = asteroid.position;

  if (hud.target_in_range && hud.beam_intensity > 0.0F && resource.integrity > 0.0F) {
    hud.beam_active = true;
    const float scaled_dt = hud.beam_intensity * dt_seconds;
    resource.integrity = std::max(0.0F, resource.integrity - (tuning.integrity_damage_per_second * scaled_dt));
    resource.extracted_mass += tuning.extraction_per_second * scaled_dt;
    resource.heat = std::min(100.0F, resource.heat + (tuning.heat_per_second * scaled_dt));
    resource.structural_stress = std::min(100.0F, resource.structural_stress + (tuning.stress_per_second * scaled_dt));
    resource.volatile_pressure = std::min(100.0F, resource.volatile_pressure + (tuning.pressure_per_second * scaled_dt));
  } else {
    resource.heat = std::max(0.0F, resource.heat - (tuning.heat_decay_per_second * dt_seconds));
    resource.structural_stress = std::max(0.0F, resource.structural_stress - (tuning.stress_relief_per_second * dt_seconds));
  }

  const bool blowout = resource.heat >= tuning.unstable_heat && resource.structural_stress >= tuning.unstable_stress &&
                       resource.volatile_pressure >= tuning.volatile_pressure_limit;
  if (blowout) {
    hud.blowout = true;
    resource.integrity = std::max(0.0F, resource.integrity - tuning.blowout_integrity_damage);
    resource.structural_stress = std::max(0.0F, resource.structural_stress * 0.35F);
    resource.volatile_pressure = 0.0F;
    resource.venting = true;
  } else {
    resource.venting = resource.heat >= tuning.unstable_heat && resource.volatile_pressure > 0.0F;
    if (resource.venting) {
      resource.volatile_pressure = std::max(0.0F, resource.volatile_pressure - (tuning.pressure_vent_per_second * dt_seconds));
    }
  }

  populate_hud_from_resource(hud, resource, tuning);
  return hud;
}

}  // namespace hyperverse
