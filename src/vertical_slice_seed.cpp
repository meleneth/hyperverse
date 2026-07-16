#include "hyperverse/vertical_slice_seed.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/cargo_train.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/hud_notice.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/projectile.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/radar_hud.hpp"
#include "hyperverse/ship_status.hpp"
#include "hyperverse/targeting.hpp"

#include <cmath>
#include <numbers>

namespace hyperverse {
namespace {

constexpr float AsteroidStartScale = 6.0F;

[[nodiscard]] AsteroidBody seed_asteroid(
  Vec2 position,
  Vec2 velocity,
  float depleted_radius,
  float angular_velocity,
  float scan_confidence
) {
  const float starting_radius = depleted_radius * AsteroidStartScale;
  return {
    .position = position,
    .velocity = velocity,
    .radius = starting_radius,
    .base_radius = starting_radius,
    .angular_velocity = angular_velocity,
    .scan_confidence = scan_confidence,
  };
}

}  // namespace

VerticalSliceEntities seed_vertical_slice(AccountCtx& account) {
  VerticalSliceEntities entities;
  entities.player = account.registry().create();
  auto& ship = account.registry().emplace<ShipMotion>(entities.player);
  ship.position = {.x = 4500.0F, .y = 4500.0F};
  account.registry().emplace<CameraState>(entities.player, CameraState{.position = ship.position});
  account.registry().emplace<ShipHealth>(entities.player);
  account.registry().emplace<ShipComputer>(entities.player);
  account.registry().emplace<RoundTimer>(entities.player);
  account.registry().emplace<HudNotice>(entities.player);
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
  account.registry().emplace<RadarHudModel>(entities.player);
  account.registry().emplace<ParticleCannonModel>(entities.player);
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
        .work_angle_radians = angle,
      }
    );
    entities.mining_drones.push_back(drone_entity);
  }

  entities.raider = account.registry().create();
  account.registry().emplace<RaiderShip>(
    entities.raider,
    RaiderShip{.position = {.x = ship.position.x + 640.0F, .y = ship.position.y - 420.0F}}
  );
  account.registry().emplace<ParticleCannonModel>(entities.raider);

  const std::vector<AsteroidBody> asteroid_field{
    seed_asteroid({.x = 5650.0F, .y = 3850.0F}, {.x = -58.0F, .y = 42.0F}, 220.0F, 0.18F, 0.34F),
    seed_asteroid({.x = 3825.0F, .y = 5200.0F}, {.x = 48.0F, .y = -32.0F}, 150.0F, -0.24F, 0.58F),
    seed_asteroid({.x = 4925.0F, .y = 2920.0F}, {.x = 34.0F, .y = 66.0F}, 95.0F, 0.42F, 0.46F),
    seed_asteroid({.x = 6200.0F, .y = 4625.0F}, {.x = -74.0F, .y = -26.0F}, 180.0F, -0.12F, 0.72F),
    seed_asteroid({.x = 3100.0F, .y = 3550.0F}, {.x = 68.0F, .y = 46.0F}, 130.0F, 0.31F, 0.41F),
    seed_asteroid({.x = 6900.0F, .y = 5980.0F}, {.x = -44.0F, .y = -70.0F}, 260.0F, 0.09F, 0.29F),
    seed_asteroid({.x = 2450.0F, .y = 6100.0F}, {.x = 82.0F, .y = -38.0F}, 110.0F, -0.36F, 0.63F),
    seed_asteroid({.x = 7800.0F, .y = 3100.0F}, {.x = -72.0F, .y = 64.0F}, 155.0F, 0.22F, 0.52F),
    seed_asteroid({.x = 1500.0F, .y = 2500.0F}, {.x = 58.0F, .y = 28.0F}, 75.0F, 0.57F, 0.38F),
    seed_asteroid({.x = 8350.0F, .y = 7250.0F}, {.x = -86.0F, .y = -44.0F}, 205.0F, -0.16F, 0.67F),
    seed_asteroid({.x = 1150.0F, .y = 7200.0F}, {.x = 46.0F, .y = -78.0F}, 140.0F, 0.28F, 0.57F),
    seed_asteroid({.x = 7350.0F, .y = 900.0F}, {.x = -34.0F, .y = 88.0F}, 100.0F, -0.44F, 0.49F),
    seed_asteroid({.x = 900.0F, .y = 4300.0F}, {.x = 92.0F, .y = -18.0F}, 125.0F, 0.36F, 0.44F),
    seed_asteroid({.x = 2250.0F, .y = 1250.0F}, {.x = 36.0F, .y = 74.0F}, 170.0F, -0.20F, 0.61F),
    seed_asteroid({.x = 4550.0F, .y = 7750.0F}, {.x = -64.0F, .y = -58.0F}, 190.0F, 0.14F, 0.53F),
    seed_asteroid({.x = 8600.0F, .y = 4550.0F}, {.x = -96.0F, .y = 24.0F}, 115.0F, -0.30F, 0.47F),
    seed_asteroid({.x = 5250.0F, .y = 8300.0F}, {.x = 52.0F, .y = -92.0F}, 145.0F, 0.25F, 0.69F),
    seed_asteroid({.x = 3450.0F, .y = 840.0F}, {.x = -42.0F, .y = 96.0F}, 85.0F, -0.52F, 0.36F),
  };

  std::size_t asteroid_index = 0;
  for (const AsteroidBody& asteroid : asteroid_field) {
    const entt::entity entity = account.registry().create();
    account.registry().emplace<AsteroidBody>(entity, asteroid);
    account.registry().emplace<AsteroidFragmentation>(entity, AsteroidFragmentation{.remaining_breaks = 2});
    account.registry().emplace<AsteroidMass>(entity, asteroid_mass_from_radius(asteroid.radius));
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
