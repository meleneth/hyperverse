#include "hyperverse/cargo_route.hpp"

namespace hyperverse {

CargoEscortRoute extraction_route_from_gathering(Vec2 gathering_position, const SectorTuning& sector) {
  return {
    .gate_position = wrap_position(
      {
        .x = gathering_position.x + (sector.width * 0.5F),
        .y = gathering_position.y + (sector.height * 0.5F),
      },
      sector
    ),
  };
}

CargoEscortRouteHudSnapshot update_cargo_escort_route(
  const CargoEscortState& escort,
  const CargoEscortRoute& route,
  const ShipMotion& ship,
  const SectorTuning& sector
) {
  const bool active = escort.phase == CargoEscortPhase::EscortActive || escort.phase == CargoEscortPhase::Extracting;
  const float distance = wrapped_distance(ship.position, route.gate_position, sector);

  return {
    .gate_position = route.gate_position,
    .gate_distance = distance,
    .active = active,
    .gate_reached = active && distance <= route.gate_radius,
  };
}

}  // namespace hyperverse
