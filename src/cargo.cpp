#include "hyperverse/cargo.hpp"

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

int sync_cargo_boxes(
  entt::registry& registry,
  const CargoManifest& manifest,
  const ExtractionSite& extraction_site,
  const CargoBoxTuning& tuning
) {
  int existing_boxes = 0;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    box.index = existing_boxes;
    box.mass = tuning.box_mass;
    box.position = {.x = extraction_site.position.x + (static_cast<float>(existing_boxes) * tuning.box_spacing), .y = extraction_site.position.y};
    ++existing_boxes;
  }

  while (existing_boxes < manifest.cargo_boxes) {
    const entt::entity box_entity = registry.create();
    registry.emplace<CargoBox>(
      box_entity,
      CargoBox{
        .position = {.x = extraction_site.position.x + (static_cast<float>(existing_boxes) * tuning.box_spacing), .y = extraction_site.position.y},
        .mass = tuning.box_mass,
        .index = existing_boxes,
      }
    );
    ++existing_boxes;
  }

  return existing_boxes;
}

CargoEscortHudSnapshot update_cargo_escort_state(
  CargoEscortState& escort,
  const CargoHudSnapshot& cargo,
  const SemanticInputFrame& input
) {
  if (escort.phase != CargoEscortPhase::EscortActive) {
    if (!cargo.extraction_authorized) {
      escort.phase = CargoEscortPhase::Mining;
    } else if (input.confirm_requested) {
      escort.phase = CargoEscortPhase::EscortActive;
    } else {
      escort.phase = CargoEscortPhase::Authorized;
    }
  }

  return {
    .phase = escort.phase,
    .extraction_authorized = cargo.extraction_authorized,
    .cargo_train_active = escort.phase == CargoEscortPhase::EscortActive,
  };
}

}  // namespace hyperverse
