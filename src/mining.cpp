#include "hyperverse/mining.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_mass.hpp"
#include "jolt_shape_queries.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {
namespace {

constexpr float AsteroidMinimumRadiusFraction = 1.0F / 6.0F;

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

  for (auto [entity, asteroid, resource] : registry.view<AsteroidBody, MiningResource>().each()) {
    if (resource.integrity <= 0.0F) {
      continue;
    }

    const Vec2 relative_position = wrapped_delta(ship.position, asteroid.position, sector);
    const ShapeQueryHit hit = jolt_raycast_shape(SpriteCollisionShape::Rock, asteroid.radius, relative_position, normalized_direction, range);
    if (!hit.hit) {
      continue;
    }

    if (hit.fraction < best_fraction) {
      best_fraction = hit.fraction;
      best = {.entity = entity, .distance = hit.fraction * range};
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

MineralComposition mineral_composition_for_tier(OreTier tier) {
  switch (tier) {
    case OreTier::Common:
      return {.silicate = 0.62F, .ferrite = 0.28F, .nickel = 0.10F};
    case OreTier::Industrial:
      return {.silicate = 0.18F, .ferrite = 0.42F, .nickel = 0.28F, .cobalt = 0.12F};
    case OreTier::Rare:
      return {.silicate = 0.12F, .ferrite = 0.16F, .nickel = 0.20F, .cobalt = 0.32F, .iridium = 0.20F};
    case OreTier::Exotic:
      return {.silicate = 0.08F, .cobalt = 0.17F, .iridium = 0.30F, .exotic_crystal = 0.45F};
    case OreTier::Anomalous:
      return {.silicate = 0.05F, .iridium = 0.18F, .exotic_crystal = 0.27F, .anomalous_matter = 0.50F};
  }

  return {};
}

const char* ore_tier_name(OreTier tier) {
  return ore_tier_profile(tier).name;
}

OreTierProfile ore_tier_profile(OreTier tier) {
  switch (tier) {
    case OreTier::Common:
      return {.name = "COMMON", .cash_per_mass = OreTierCashPerMass[0], .tint = {.r = 0.62F, .g = 0.66F, .b = 0.70F}};
    case OreTier::Industrial:
      return {.name = "INDUSTRIAL", .cash_per_mass = OreTierCashPerMass[1], .tint = {.r = 0.95F, .g = 0.70F, .b = 0.34F}};
    case OreTier::Rare:
      return {.name = "RARE", .cash_per_mass = OreTierCashPerMass[2], .tint = {.r = 0.28F, .g = 0.78F, .b = 1.0F}};
    case OreTier::Exotic:
      return {.name = "EXOTIC", .cash_per_mass = OreTierCashPerMass[3], .tint = {.r = 1.0F, .g = 0.42F, .b = 0.95F}};
    case OreTier::Anomalous:
      return {.name = "ANOMALOUS", .cash_per_mass = OreTierCashPerMass[4], .tint = {.r = 0.28F, .g = 1.0F, .b = 0.58F}};
  }

  return {};
}

float ore_tier_cash_per_mass(OreTier tier) {
  return ore_tier_profile(tier).cash_per_mass;
}

OreTint ore_tint(OreTier tier) {
  return ore_tier_profile(tier).tint;
}

OreTint ore_tint(const MineralComposition& composition) {
  return {
    .r = std::clamp(
      0.48F + (composition.ferrite * 0.35F) + (composition.nickel * 0.28F) + (composition.iridium * 0.55F) +
        (composition.exotic_crystal * 0.80F) + (composition.anomalous_matter * -0.12F),
      0.0F,
      1.0F
    ),
    .g = std::clamp(
      0.50F + (composition.silicate * 0.30F) + (composition.cobalt * 0.45F) + (composition.anomalous_matter * 1.0F),
      0.0F,
      1.0F
    ),
    .b = std::clamp(
      0.52F + (composition.nickel * 0.16F) + (composition.cobalt * 0.48F) + (composition.iridium * 0.35F) +
        (composition.exotic_crystal * 0.62F) + (composition.anomalous_matter * -0.16F),
      0.0F,
      1.0F
    ),
  };
}

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

  AsteroidBody& asteroid = registry.get<AsteroidBody>(target.entity);
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

  const float remaining_fraction = std::clamp(resource.integrity / 100.0F, AsteroidMinimumRadiusFraction, 1.0F);
  sync_asteroid_mass_to_integrity(registry, target.entity, resource.integrity / 100.0F);
  asteroid.radius = std::max(MinimumPlayableAsteroidRadius, asteroid.base_radius * remaining_fraction);
  if (resource.integrity <= 0.0F) {
    populate_hud_from_resource(hud, resource, tuning);
    (void)fragment_asteroid(
      registry,
      target.entity,
      AsteroidFragmentationRequest{
        .impact_kind = AsteroidImpactKind::Laser,
        .impact_position = ship.position,
        .impact_velocity = facing_direction(ship.facing_radians) * tuning.range,
        .pieces = 4,
      }
    );
    return hud;
  }
  populate_hud_from_resource(hud, resource, tuning);
  return hud;
}

}  // namespace hyperverse
