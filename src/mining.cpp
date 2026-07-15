#include "hyperverse/mining.hpp"

#include <algorithm>

namespace hyperverse {

MiningHudSnapshot update_mining_laser(
  entt::registry& registry,
  const TargetLockModel& target_lock,
  const SemanticInputFrame& input,
  const MiningLaserTuning& tuning,
  float dt_seconds
) {
  MiningHudSnapshot hud{};
  if (!has_locked_target(target_lock) || !registry.valid(target_lock.target) || !registry.all_of<MiningResource>(target_lock.target)) {
    return hud;
  }

  MiningResource& resource = registry.get<MiningResource>(target_lock.target);
  hud.target_in_range = target_lock.wrapped_distance <= tuning.range;
  hud.beam_intensity = std::clamp(input.tool_intensity, 0.0F, 1.0F);

  if (hud.target_in_range && hud.beam_intensity > 0.0F && resource.integrity > 0.0F) {
    hud.beam_active = true;
    const float scaled_dt = hud.beam_intensity * dt_seconds;
    resource.integrity = std::max(0.0F, resource.integrity - (tuning.integrity_damage_per_second * scaled_dt));
    resource.extracted_mass += tuning.extraction_per_second * scaled_dt;
    resource.heat = std::min(100.0F, resource.heat + (tuning.heat_per_second * scaled_dt));
  } else {
    resource.heat = std::max(0.0F, resource.heat - (tuning.heat_decay_per_second * dt_seconds));
  }

  hud.target_integrity = resource.integrity;
  hud.target_heat = resource.heat;
  hud.extracted_mass = resource.extracted_mass;
  return hud;
}

}  // namespace hyperverse
