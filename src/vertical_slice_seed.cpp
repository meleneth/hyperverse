#include "hyperverse/vertical_slice_seed.hpp"

#include "hyperverse/camera.hpp"
#include "hyperverse/cargo.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/projectile.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/targeting.hpp"

#include <cmath>
#include <numbers>

namespace hyperverse {

VerticalSliceEntities seed_vertical_slice(AccountCtx& account) {
  VerticalSliceEntities entities;
  entities.player = account.registry().create();
  auto& ship = account.registry().emplace<ShipMotion>(entities.player);
  ship.position = {.x = 4500.0F, .y = 4500.0F};
  account.registry().emplace<CameraState>(entities.player, CameraState{.position = ship.position});
  account.registry().emplace<TargetLockModel>(entities.player);
  account.registry().emplace<MiningHudSnapshot>(entities.player);
  account.registry().emplace<CargoManifest>(entities.player);
  account.registry().emplace<CargoHudSnapshot>(entities.player);
  account.registry().emplace<CargoEscortState>(entities.player);
  account.registry().emplace<CargoEscortHudSnapshot>(entities.player);
  account.registry().emplace<CargoTrainHudSnapshot>(entities.player);
  account.registry().emplace<CargoEscortRouteHudSnapshot>(entities.player);
  account.registry().emplace<SectorPressureModel>(entities.player);
  account.registry().emplace<SectorPressureHudSnapshot>(entities.player);
  account.registry().emplace<MiningDroneHudSnapshot>(entities.player);
  account.registry().emplace<ParticleCannonHudSnapshot>(entities.player);
  account.event_bus().appendListener(DomainEventType::ParticleImpact, [&account, player = entities.player](const DomainEvent&) {
    account.registry().get<ParticleCannonHudSnapshot>(player).impacts += 1;
  });
  account.registry().emplace<RaiderHudSnapshot>(entities.player);
  account.registry().emplace<CargoRecoveryHudSnapshot>(entities.player);
  account.registry().emplace<CollisionHudSnapshot>(entities.player);

  entities.mining_drones.reserve(8);
  for (int index = 0; index < 8; ++index) {
    const float angle = (static_cast<float>(index) / 8.0F) * std::numbers::pi_v<float> * 2.0F;
    const entt::entity drone_entity = account.registry().create();
    account.registry().emplace<MiningDrone>(
      drone_entity,
      MiningDrone{
        .position = {.x = ship.position.x + (std::cos(angle) * 180.0F), .y = ship.position.y + (std::sin(angle) * 180.0F)},
        .facing_radians = angle,
      }
    );
    entities.mining_drones.push_back(drone_entity);
  }

  entities.raider = account.registry().create();
  account.registry().emplace<RaiderShip>(
    entities.raider,
    RaiderShip{.position = {.x = ship.position.x + 640.0F, .y = ship.position.y - 420.0F}}
  );

  const std::vector<AsteroidBody> asteroid_field{
    {.position = {.x = 5650.0F, .y = 3850.0F}, .velocity = {.x = -22.0F, .y = 16.0F}, .radius = 220.0F, .base_radius = 220.0F, .angular_velocity = 0.18F, .scan_confidence = 0.34F},
    {.position = {.x = 3825.0F, .y = 5200.0F}, .velocity = {.x = 18.0F, .y = -10.0F}, .radius = 150.0F, .base_radius = 150.0F, .angular_velocity = -0.24F, .scan_confidence = 0.58F},
    {.position = {.x = 4925.0F, .y = 2920.0F}, .velocity = {.x = 12.0F, .y = 22.0F}, .radius = 95.0F, .base_radius = 95.0F, .angular_velocity = 0.42F, .scan_confidence = 0.46F},
    {.position = {.x = 6200.0F, .y = 4625.0F}, .velocity = {.x = -30.0F, .y = -8.0F}, .radius = 180.0F, .base_radius = 180.0F, .angular_velocity = -0.12F, .scan_confidence = 0.72F},
    {.position = {.x = 3100.0F, .y = 3550.0F}, .velocity = {.x = 26.0F, .y = 19.0F}, .radius = 130.0F, .base_radius = 130.0F, .angular_velocity = 0.31F, .scan_confidence = 0.41F},
    {.position = {.x = 6900.0F, .y = 5980.0F}, .velocity = {.x = -18.0F, .y = -24.0F}, .radius = 260.0F, .base_radius = 260.0F, .angular_velocity = 0.09F, .scan_confidence = 0.29F},
    {.position = {.x = 2450.0F, .y = 6100.0F}, .velocity = {.x = 34.0F, .y = -14.0F}, .radius = 110.0F, .base_radius = 110.0F, .angular_velocity = -0.36F, .scan_confidence = 0.63F},
    {.position = {.x = 7800.0F, .y = 3100.0F}, .velocity = {.x = -28.0F, .y = 27.0F}, .radius = 155.0F, .base_radius = 155.0F, .angular_velocity = 0.22F, .scan_confidence = 0.52F},
    {.position = {.x = 1500.0F, .y = 2500.0F}, .velocity = {.x = 24.0F, .y = 11.0F}, .radius = 75.0F, .base_radius = 75.0F, .angular_velocity = 0.57F, .scan_confidence = 0.38F},
    {.position = {.x = 8350.0F, .y = 7250.0F}, .velocity = {.x = -36.0F, .y = -18.0F}, .radius = 205.0F, .base_radius = 205.0F, .angular_velocity = -0.16F, .scan_confidence = 0.67F},
    {.position = {.x = 1150.0F, .y = 7200.0F}, .velocity = {.x = 20.0F, .y = -32.0F}, .radius = 140.0F, .base_radius = 140.0F, .angular_velocity = 0.28F, .scan_confidence = 0.57F},
    {.position = {.x = 7350.0F, .y = 900.0F}, .velocity = {.x = -12.0F, .y = 36.0F}, .radius = 100.0F, .base_radius = 100.0F, .angular_velocity = -0.44F, .scan_confidence = 0.49F},
  };

  std::size_t asteroid_index = 0;
  for (const AsteroidBody& asteroid : asteroid_field) {
    const entt::entity entity = account.registry().create();
    account.registry().emplace<AsteroidBody>(entity, asteroid);
    const OreTier tier =
      asteroid_index == 5U ? OreTier::Anomalous :
      asteroid_index == 9U ? OreTier::Exotic :
      (asteroid_index == 3U || asteroid_index == 7U) ? OreTier::Rare :
      (asteroid_index == 1U || asteroid_index == 4U || asteroid_index == 10U) ? OreTier::Industrial :
      OreTier::Common;
    account.registry().emplace<MiningResource>(entity, MiningResource{.tier = tier});
    account.registry().emplace<MineralComposition>(entity, mineral_composition_for_tier(tier));
    ++asteroid_index;
  }

  return entities;
}

}  // namespace hyperverse
