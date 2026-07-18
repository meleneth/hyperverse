#pragma once

#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class RaiderPhase {
  Idle,
  Approaching,
  Disrupting,
  Towing,
  Escaped,
};

enum class RaiderRole {
  CargoThief,
  Combat,
};

enum class RaiderTask {
  HarassPlayer,
  CoverThief,
  StealCargo,
  FullAggression,
};

struct RaiderShip {
  Vec2 position{};
  Vec2 velocity{};
  entt::entity target_box{entt::null};
  RaiderPhase phase{RaiderPhase::Idle};
  RaiderRole role{RaiderRole::CargoThief};
  RaiderTask task{RaiderTask::StealCargo};
  float disruption_seconds{0.0F};
  float integrity{120.0F};
  float max_integrity{120.0F};
  float facing_radians{0.0F};
  float orbit_radians{0.0F};
  float cloak_fade_seconds{0.0F};
};

struct RaiderTuning {
  float max_speed{460.0F};
  float disruption_range{70.0F};
  float disruption_seconds{0.5F};
  float escape_distance{3200.0F};
  float combat_standoff{900.0F};
  float combat_orbit_x_radius{1180.0F};
  float combat_orbit_y_radius{620.0F};
  float combat_orbit_radians_per_second{0.72F};
  float combat_orbit_arrival_tolerance{90.0F};
  float combat_acceleration{720.0F};
  float combat_damping{0.92F};
  float cloak_fade_seconds{1.15F};
};

void spawn_gate_combat_raiders(
  entt::registry& registry,
  Vec2 gate_position,
  Vec2 player_position,
  const SectorTuning& sector,
  int count = 3
);

int spawn_pressure_raiders(
  entt::registry& registry,
  Vec2 player_position,
  const SectorTuning& sector,
  int threat_level
);

struct RaiderHudSnapshot {
  entt::entity target_box{entt::null};
  RaiderPhase phase{RaiderPhase::Idle};
  RaiderTask task{RaiderTask::StealCargo};
  float target_distance{0.0F};
  float disruption_fraction{0.0F};
  float escape_distance{0.0F};
  bool active{false};
};

struct CargoRecoveryTuning {
  float recovery_range{120.0F};
};

struct CargoRecoveryHudSnapshot {
  entt::entity recovered_box{entt::null};
  float nearest_stolen_distance{0.0F};
  bool stolen_box_near{false};
  bool recovered{false};
};

[[nodiscard]] RaiderHudSnapshot update_raider_threat(
  RaiderShip& raider,
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning = {}
);

[[nodiscard]] CargoRecoveryHudSnapshot recover_stolen_cargo(
  entt::registry& registry,
  RaiderShip& raider,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const CargoRecoveryTuning& tuning = {}
);

}  // namespace hyperverse
