#include "hyperverse/vertical_slice_seed.hpp"

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/asteroid_geometry.hpp"
#include "hyperverse/asteroid_mass.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/cargo_train.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/engine_trail.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/gravity_sling.hpp"
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
#include <vector>

namespace hyperverse {
namespace {

constexpr float AsteroidStartScale = 6.0F;

[[nodiscard]] float hash_unit(std::uint32_t seed) {
  seed ^= seed >> 16U;
  seed *= 0x7FEB352DU;
  seed ^= seed >> 15U;
  seed *= 0x846CA68BU;
  seed ^= seed >> 16U;
  return static_cast<float>(seed >> 8U) * (1.0F / 16777215.0F);
}

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
    .angular_velocity = angular_velocity * 2.4F,
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
  account.registry().emplace<GravitySlingModel>(entities.player);
  account.registry().emplace<GravitySlingHudSnapshot>(entities.player);
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
  account.registry().emplace<EngineTrailModel>(entities.player);

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
    account.registry().emplace<ParticleCannonModel>(drone_entity);
    account.registry().emplace<EngineTrailModel>(drone_entity);
    entities.mining_drones.push_back(drone_entity);
  }

  entities.raider = account.registry().create();
  account.registry().emplace<RaiderShip>(
    entities.raider,
    RaiderShip{.position = {.x = ship.position.x + 640.0F, .y = ship.position.y - 420.0F}}
  );
  account.registry().emplace<ParticleCannonModel>(entities.raider);
  account.registry().emplace<EngineTrailModel>(entities.raider);

  std::vector<AsteroidBody> asteroid_field;
  asteroid_field.reserve(72);
  const SectorTuning sector = default_sector();
  for (int row = 0; row < 8; ++row) {
    for (int column = 0; column < 9; ++column) {
      const int index = (row * 9) + column;
      const float jitter_x = (hash_unit(0xA5100000U + static_cast<std::uint32_t>(index * 3)) - 0.5F) * (sector.width / 11.0F);
      const float jitter_y = (hash_unit(0xA5100001U + static_cast<std::uint32_t>(index * 3)) - 0.5F) * (sector.height / 10.0F);
      const float position_x = ((static_cast<float>(column) + 0.5F) / 9.0F) * sector.width + jitter_x;
      const float position_y = ((static_cast<float>(row) + 0.5F) / 8.0F) * sector.height + jitter_y;
      const float angle = hash_unit(0xA5100002U + static_cast<std::uint32_t>(index * 5)) * std::numbers::pi_v<float> * 2.0F;
      const float speed = 420.0F + (hash_unit(0xA5100003U + static_cast<std::uint32_t>(index * 5)) * 360.0F);
      const float depleted_radius = 75.0F + (hash_unit(0xA5100004U + static_cast<std::uint32_t>(index * 7)) * 190.0F);
      const float spin_sign = hash_unit(0xA5100005U + static_cast<std::uint32_t>(index * 7)) < 0.5F ? -1.0F : 1.0F;
      const float angular_velocity = spin_sign * (0.28F + (hash_unit(0xA5100007U + static_cast<std::uint32_t>(index * 7)) * 0.72F));
      asteroid_field.push_back(seed_asteroid(
        wrap_position({.x = position_x, .y = position_y}, sector),
        {.x = std::cos(angle) * speed, .y = std::sin(angle) * speed},
        depleted_radius,
        angular_velocity,
        0.28F + (hash_unit(0xA5100006U + static_cast<std::uint32_t>(index * 11)) * 0.48F)
      ));
    }
  }

  std::size_t asteroid_index = 0;
  for (const AsteroidBody& asteroid : asteroid_field) {
    const entt::entity entity = account.registry().create();
    account.registry().emplace<AsteroidBody>(entity, asteroid);
    account.registry().emplace<AsteroidGeometry>(entity, generate_asteroid_geometry(static_cast<std::uint32_t>(0xA57E0000U + asteroid_index), asteroid.radius));
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
