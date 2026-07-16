#include "hyperverse/raider.hpp"

#include <algorithm>
#include <limits>

namespace hyperverse {

namespace {

[[nodiscard]] entt::entity rearmost_cargo_box(entt::registry& registry) {
  entt::entity selected = entt::null;
  int selected_index = std::numeric_limits<int>::min();

  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    if (box.index > selected_index) {
      selected = entity;
      selected_index = box.index;
    }
  }

  return selected;
}

}  // namespace

RaiderHudSnapshot update_raider_threat(
  RaiderShip& raider,
  entt::registry& registry,
  const CargoEscortState& escort,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning
) {
  if (escort.phase != CargoEscortPhase::EscortActive) {
    raider.phase = RaiderPhase::Idle;
    raider.velocity = {};
    raider.target_box = entt::null;
    raider.disruption_seconds = 0.0F;
    return {};
  }

  if (raider.target_box == entt::null || !registry.valid(raider.target_box) || !registry.all_of<CargoBox>(raider.target_box)) {
    raider.target_box = rearmost_cargo_box(registry);
  }

  if (raider.target_box == entt::null) {
    raider.phase = RaiderPhase::Idle;
    raider.velocity = {};
    raider.disruption_seconds = 0.0F;
    return {.active = true};
  }

  const CargoBox& target = registry.get<CargoBox>(raider.target_box);
  const Vec2 to_target = wrapped_delta(raider.position, target.position, sector);
  const float target_distance = length(to_target);

  if (target_distance > tuning.disruption_range) {
    raider.phase = RaiderPhase::Approaching;
    raider.disruption_seconds = 0.0F;
    raider.velocity = normalize_or_zero(to_target) * tuning.max_speed;
    raider.position = wrap_position(raider.position + (raider.velocity * dt_seconds), sector);
  } else {
    raider.phase = RaiderPhase::Disrupting;
    raider.velocity = {};
    raider.disruption_seconds = std::min(tuning.disruption_seconds, raider.disruption_seconds + dt_seconds);
  }

  return {
    .target_box = raider.target_box,
    .phase = raider.phase,
    .target_distance = target_distance,
    .disruption_fraction = raider.disruption_seconds / std::max(tuning.disruption_seconds, std::numeric_limits<float>::epsilon()),
    .active = true,
  };
}

}  // namespace hyperverse
