#include "hyperverse/cargo_manifest.hpp"

#include "hyperverse/mining.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {

CargoHudSnapshot update_cargo_manifest(
  CargoManifest& manifest,
  entt::registry& registry,
  const ContractQuotaTuning& tuning
) {
  float delivered_mass = 0.0F;
  for (auto [entity, resource] : registry.view<MiningResource>().each()) {
    (void)entity;
    delivered_mass += resource.extracted_mass;
  }

  const float box_mass = std::max(tuning.cargo_box_mass, std::numeric_limits<float>::epsilon());
  manifest.delivered_mass = delivered_mass;
  manifest.cargo_boxes = static_cast<int>(std::floor(delivered_mass / box_mass));

  const float required_mass = std::max(tuning.required_mass, std::numeric_limits<float>::epsilon());
  const float over_quota_mass = std::max(0.0F, delivered_mass - tuning.required_mass);
  const float bonus_step = std::max(tuning.over_quota_bonus_step_mass, std::numeric_limits<float>::epsilon());
  const float bonus_steps = std::floor(over_quota_mass / bonus_step);

  return {
    .delivered_mass = manifest.delivered_mass,
    .required_mass = tuning.required_mass,
    .quota_fraction = std::clamp(delivered_mass / required_mass, 0.0F, 1.0F),
    .over_quota_mass = over_quota_mass,
    .payout_multiplier = 1.0F + (bonus_steps * tuning.bonus_per_step),
    .cargo_boxes = manifest.cargo_boxes,
    .extraction_authorized = delivered_mass >= tuning.required_mass,
  };
}

}  // namespace hyperverse
