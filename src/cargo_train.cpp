#include "hyperverse/cargo_train.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace hyperverse {

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
    if (box.state == CargoBoxState::Linked) {
      boxes.push_back(&box);
    }
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

}  // namespace hyperverse
