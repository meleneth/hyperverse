#include "hyperverse/cargo_route.hpp"

namespace hyperverse {

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
