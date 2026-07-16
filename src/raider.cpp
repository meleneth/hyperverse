#include "hyperverse/raider.hpp"

#include "hyperverse/projectile.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {

namespace {

[[nodiscard]] entt::entity rearmost_cargo_box(entt::registry& registry) {
  entt::entity selected = entt::null;
  int selected_index = std::numeric_limits<int>::min();

  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    if (box.state == CargoBoxState::Linked && box.index > selected_index) {
      selected = entity;
      selected_index = box.index;
    }
  }

  return selected;
}

}  // namespace

void spawn_gate_combat_raiders(
  entt::registry& registry,
  Vec2 gate_position,
  Vec2 player_position,
  const SectorTuning& sector,
  int count
) {
  const int spawn_count = std::max(0, count);
  const Vec2 away_from_player = normalize_or_zero(wrapped_delta(player_position, gate_position, sector));
  const Vec2 base_direction = length(away_from_player) > 0.0F ? away_from_player : Vec2{.x = 1.0F, .y = 0.0F};
  const float base_angle = std::atan2(base_direction.y, base_direction.x);

  for (int index = 0; index < spawn_count; ++index) {
    const float offset_angle = base_angle + ((static_cast<float>(index) - ((static_cast<float>(spawn_count) - 1.0F) * 0.5F)) * 0.55F);
    const Vec2 offset{.x = std::cos(offset_angle) * 720.0F, .y = std::sin(offset_angle) * 720.0F};
    const entt::entity raider = registry.create();
    registry.emplace<RaiderShip>(
      raider,
      RaiderShip{
        .position = wrap_position(gate_position + offset, sector),
        .role = RaiderRole::Combat,
        .integrity = 90.0F,
        .max_integrity = 90.0F,
      }
    );
    registry.emplace<ParticleCannonModel>(raider);
  }
}

RaiderHudSnapshot update_raider_threat(
  RaiderShip& raider,
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning
) {
  if (raider.integrity <= 0.0F) {
    raider.phase = RaiderPhase::Idle;
    raider.velocity = {};
    raider.target_box = entt::null;
    raider.disruption_seconds = 0.0F;
    return {};
  }

  if (raider.role == RaiderRole::Combat) {
    const Vec2 to_player = wrapped_delta(raider.position, ship.position, sector);
    const float player_distance = length(to_player);
    if (player_distance > tuning.combat_standoff) {
      raider.phase = RaiderPhase::Approaching;
      raider.velocity = normalize_or_zero(to_player) * tuning.max_speed;
      raider.position = wrap_position(raider.position + (raider.velocity * dt_seconds), sector);
    } else {
      raider.phase = RaiderPhase::Disrupting;
      raider.velocity = {};
    }
    return {
      .phase = raider.phase,
      .target_distance = player_distance,
      .active = true,
    };
  }

  if (escort.phase != CargoEscortPhase::EscortActive) {
    if (raider.phase != RaiderPhase::Escaped) {
      raider.phase = RaiderPhase::Idle;
    }
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

  CargoBox& target = registry.get<CargoBox>(raider.target_box);
  const Vec2 to_target = wrapped_delta(raider.position, target.position, sector);
  const float target_distance = length(to_target);

  if (target.state == CargoBoxState::Stolen || raider.phase == RaiderPhase::Towing) {
    raider.phase = RaiderPhase::Towing;
  } else if (target_distance > tuning.disruption_range) {
    raider.phase = RaiderPhase::Approaching;
    raider.disruption_seconds = 0.0F;
    raider.velocity = normalize_or_zero(to_target) * tuning.max_speed;
    raider.position = wrap_position(raider.position + (raider.velocity * dt_seconds), sector);
  } else {
    raider.phase = RaiderPhase::Disrupting;
    raider.velocity = {};
    raider.disruption_seconds = std::min(tuning.disruption_seconds, raider.disruption_seconds + dt_seconds);
    if (raider.disruption_seconds >= tuning.disruption_seconds) {
      target.state = CargoBoxState::Stolen;
      raider.phase = RaiderPhase::Towing;
    }
  }

  if (raider.phase == RaiderPhase::Towing) {
    Vec2 escape_direction = normalize_or_zero(wrapped_delta(ship.position, raider.position, sector));
    if (length(escape_direction) <= 0.0001F) {
      escape_direction = {.x = 1.0F, .y = 0.0F};
    }
    raider.velocity = escape_direction * tuning.max_speed;
    raider.position = wrap_position(raider.position + (raider.velocity * dt_seconds), sector);
    target.position = raider.position;
    target.velocity = raider.velocity;
    if (wrapped_distance(ship.position, target.position, sector) >= tuning.escape_distance) {
      target.state = CargoBoxState::Lost;
      raider.phase = RaiderPhase::Escaped;
      raider.target_box = entt::null;
      raider.velocity = {};
    }
  }

  return {
    .target_box = raider.target_box,
    .phase = raider.phase,
    .target_distance = target_distance,
    .disruption_fraction = raider.disruption_seconds / std::max(tuning.disruption_seconds, std::numeric_limits<float>::epsilon()),
    .escape_distance =
      (raider.phase == RaiderPhase::Towing || raider.phase == RaiderPhase::Escaped) ? wrapped_distance(ship.position, target.position, sector) : 0.0F,
    .active = true,
  };
}

CargoRecoveryHudSnapshot recover_stolen_cargo(
  entt::registry& registry,
  RaiderShip& raider,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const CargoRecoveryTuning& tuning
) {
  entt::entity nearest = entt::null;
  float nearest_distance = std::numeric_limits<float>::max();

  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    if (box.state != CargoBoxState::Stolen) {
      continue;
    }

    const float distance = wrapped_distance(ship.position, box.position, sector);
    if (distance < nearest_distance) {
      nearest = entity;
      nearest_distance = distance;
    }
  }

  if (nearest == entt::null) {
    return {};
  }

  CargoRecoveryHudSnapshot hud{
    .recovered_box = nearest,
    .nearest_stolen_distance = nearest_distance,
    .stolen_box_near = nearest_distance <= tuning.recovery_range,
  };

  if (hud.stolen_box_near && input.confirm_requested) {
    CargoBox& box = registry.get<CargoBox>(nearest);
    box.state = CargoBoxState::Linked;
    box.velocity = ship.velocity;

    if (raider.target_box == nearest) {
      raider.target_box = entt::null;
      raider.phase = RaiderPhase::Idle;
      raider.disruption_seconds = 0.0F;
    }

    hud.recovered = true;
  }

  return hud;
}

}  // namespace hyperverse
