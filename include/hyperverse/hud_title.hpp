#pragma once

#include "hyperverse/camera.hpp"
#include "hyperverse/cargo.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/targeting.hpp"

#include <string>

namespace hyperverse {

[[nodiscard]] std::string make_title(
  const FlightHudSnapshot& hud,
  const CameraState& camera,
  const TargetLockModel& target_lock,
  const MiningHudSnapshot& mining,
  const CargoHudSnapshot& cargo,
  const CargoEscortHudSnapshot& escort,
  const CargoTrainHudSnapshot& train,
  const CargoEscortRouteHudSnapshot& route,
  const SectorPressureHudSnapshot& pressure,
  const MiningDroneHudSnapshot& drone,
  const RaiderHudSnapshot& raider,
  const CargoRecoveryHudSnapshot& recovery,
  const CollisionHudSnapshot& collision
);

}  // namespace hyperverse
