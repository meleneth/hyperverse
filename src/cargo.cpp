#include "hyperverse/cargo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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

CargoTrainHudSnapshot update_cargo_train(
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const CargoTrainTuning& tuning
) {
  std::vector<CargoBox*> boxes;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    boxes.push_back(&box);
  }
  std::ranges::sort(boxes, [](const CargoBox* lhs, const CargoBox* rhs) { return lhs->index < rhs->index; });

  CargoTrainHudSnapshot hud{
    .linked_boxes = static_cast<int>(boxes.size()),
    .train_length = static_cast<float>(boxes.size()) * tuning.link_spacing,
    .active = escort.phase == CargoEscortPhase::EscortActive,
  };

  if (!hud.active || boxes.empty()) {
    return hud;
  }

  const Vec2 facing_direction = normalize_or_zero({.x = std::cos(ship.facing_radians), .y = std::sin(ship.facing_radians)});
  const Vec2 travel_direction = length(ship.velocity) > 25.0F ? normalize_or_zero(ship.velocity) : facing_direction;
  Vec2 anchor = ship.position;

  for (CargoBox* box : boxes) {
    const Vec2 target = wrap_position(anchor - (travel_direction * tuning.link_spacing), sector);
    const Vec2 target_delta = wrapped_delta(box->position, target, sector);
    const float link_error = length(target_delta);
    hud.max_coupling_stress = std::max(hud.max_coupling_stress, link_error / std::max(tuning.link_spacing, std::numeric_limits<float>::epsilon()));

    box->velocity = clamp_length(target_delta * tuning.follow_rate, tuning.max_speed);
    box->position = wrap_position(box->position + (box->velocity * dt_seconds), sector);
    anchor = box->position;
  }

  return hud;
}

CargoEscortRouteHudSnapshot update_cargo_escort_route(
  const CargoEscortState& escort,
  const CargoEscortRoute& route,
  const ShipMotion& ship,
  const SectorTuning& sector
) {
  const bool active = escort.phase == CargoEscortPhase::EscortActive;
  const float distance = wrapped_distance(ship.position, route.gate_position, sector);

  return {
    .gate_position = route.gate_position,
    .gate_distance = distance,
    .active = active,
    .gate_reached = active && distance <= route.gate_radius,
  };
}

}  // namespace hyperverse
