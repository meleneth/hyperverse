#pragma once

#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

struct CargoTrainTuning {
  float link_spacing{86.0F};
  float follow_rate{5.0F};
  float max_speed{520.0F};
};

struct CargoTrainHudSnapshot {
  int linked_boxes{0};
  float train_length{0.0F};
  float max_coupling_stress{0.0F};
  bool active{false};
};

[[nodiscard]] CargoTrainHudSnapshot update_cargo_train(
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const CargoTrainTuning& tuning = {}
);

int detach_linked_cargo(entt::registry& registry, Vec2 inherited_velocity);

}  // namespace hyperverse
