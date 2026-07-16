#include "hyperverse/raider.hpp"

#include "hyperverse/projectile.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

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

[[nodiscard]] bool cargo_thief_needs_cover(entt::registry& registry) {
  for (auto [entity, raider] : registry.view<RaiderShip>().each()) {
    (void)entity;
    if (
      raider.role == RaiderRole::CargoThief &&
      (raider.phase == RaiderPhase::Disrupting || raider.phase == RaiderPhase::Towing)
    ) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] RaiderTask select_raider_task(const RaiderShip& raider, entt::registry& registry, const CargoEscortState& escort) {
  if (raider.role == RaiderRole::CargoThief) {
    return RaiderTask::StealCargo;
  }
  if (raider.task == RaiderTask::FullAggression) {
    return RaiderTask::FullAggression;
  }
  if (escort.phase == CargoEscortPhase::Extracting || escort.phase == CargoEscortPhase::Complete) {
    return RaiderTask::FullAggression;
  }
  if (cargo_thief_needs_cover(registry)) {
    return RaiderTask::CoverThief;
  }
  return RaiderTask::HarassPlayer;
}

void spawn_combat_raider(entt::registry& registry, Vec2 position, RaiderTask task, float integrity) {
  const entt::entity raider = registry.create();
  registry.emplace<RaiderShip>(
    raider,
    RaiderShip{
      .position = position,
      .role = RaiderRole::Combat,
      .task = task,
      .integrity = integrity,
      .max_integrity = integrity,
    }
  );
  registry.emplace<ParticleCannonModel>(raider);
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
    spawn_combat_raider(registry, wrap_position(gate_position + offset, sector), RaiderTask::FullAggression, 90.0F);
  }
}

int spawn_pressure_raiders(
  entt::registry& registry,
  Vec2 player_position,
  const SectorTuning& sector,
  int threat_level
) {
  if (threat_level <= 0) {
    return 0;
  }

  const int spawn_count = std::clamp(1 + (threat_level / 2), 1, 5);
  const RaiderTask task = threat_level >= 5 ? RaiderTask::FullAggression : RaiderTask::HarassPlayer;
  const float integrity = static_cast<float>(70 + (threat_level * 6));
  const float radius = 1200.0F + (static_cast<float>(threat_level) * 90.0F);
  const float base_angle = static_cast<float>(threat_level) * 0.73F;

  for (int index = 0; index < spawn_count; ++index) {
    const float angle = base_angle + ((static_cast<float>(index) / static_cast<float>(spawn_count)) * std::numbers::pi_v<float> * 2.0F);
    const Vec2 offset{.x = std::cos(angle) * radius, .y = std::sin(angle) * radius};
    spawn_combat_raider(registry, wrap_position(player_position + offset, sector), task, integrity);
  }

  return spawn_count;
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

  raider.task = select_raider_task(raider, registry, escort);
  if (raider.role == RaiderRole::Combat) {
    const Vec2 to_player = wrapped_delta(raider.position, ship.position, sector);
    const float player_distance = length(to_player);
    const float standoff =
      raider.task == RaiderTask::FullAggression ? tuning.combat_standoff * 0.55F :
      raider.task == RaiderTask::CoverThief ? tuning.combat_standoff * 1.25F :
      tuning.combat_standoff;
    if (player_distance > standoff) {
      raider.phase = RaiderPhase::Approaching;
      raider.velocity = normalize_or_zero(to_player) * tuning.max_speed;
      raider.position = wrap_position(raider.position + (raider.velocity * dt_seconds), sector);
    } else {
      raider.phase = RaiderPhase::Disrupting;
      raider.velocity = {};
    }
    return {
      .phase = raider.phase,
      .task = raider.task,
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
    .task = raider.task,
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
