#pragma once

#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/sector.hpp"

namespace hyperverse {

struct CargoEscortRoute {
  Vec2 gate_position{};
  float gate_radius{160.0F};
};

struct CargoEscortRouteHudSnapshot {
  Vec2 gate_position{};
  float gate_distance{0.0F};
  bool active{false};
  bool gate_reached{false};
};

[[nodiscard]] CargoEscortRoute extraction_route_from_gathering(Vec2 gathering_position, const SectorTuning& sector);

[[nodiscard]] CargoEscortRouteHudSnapshot update_cargo_escort_route(
  const CargoEscortState& escort,
  const CargoEscortRoute& route,
  const ShipMotion& ship,
  const SectorTuning& sector
);

}  // namespace hyperverse
