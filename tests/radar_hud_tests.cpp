#include "test_common.hpp"

#include <algorithm>

namespace {

entt::entity asteroid_at(entt::registry& registry, hyperverse::Vec2 position) {
  const entt::entity entity = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(entity, hyperverse::AsteroidBody{.position = position, .radius = 40.0F});
  return entity;
}

}  // namespace

TEST_CASE("radar tracks only the nearest targets inside range") {
  entt::registry registry;
  const entt::entity nearest = asteroid_at(registry, {.x = 120.0F, .y = 100.0F});
  const entt::entity middle = asteroid_at(registry, {.x = 180.0F, .y = 100.0F});
  (void)asteroid_at(registry, {.x = 260.0F, .y = 100.0F});
  (void)asteroid_at(registry, {.x = 1000.0F, .y = 100.0F});
  hyperverse::RadarHudModel radar;

  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    {.max_targets = 2, .range_world = 200.0F}
  );

  REQUIRE(radar.tracked_targets.size() == 2);
  CHECK(radar.tracked_targets[0].target == nearest);
  CHECK(radar.tracked_targets[1].target == middle);
}

TEST_CASE("radar target list updates on a computer cadence") {
  entt::registry registry;
  const entt::entity original = asteroid_at(registry, {.x = 180.0F, .y = 100.0F});
  hyperverse::RadarHudModel radar;

  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    {.max_targets = 1, .update_interval_seconds = 0.25F, .range_world = 500.0F}
  );
  const entt::entity newer = asteroid_at(registry, {.x = 120.0F, .y = 100.0F});

  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.24F,
    {.max_targets = 1, .update_interval_seconds = 0.25F, .range_world = 500.0F}
  );
  REQUIRE(radar.tracked_targets.size() == 1);
  CHECK(radar.tracked_targets[0].target == original);

  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.01F,
    {.max_targets = 1, .update_interval_seconds = 0.25F, .range_world = 500.0F}
  );
  REQUIRE(radar.tracked_targets.size() == 1);
  CHECK(radar.tracked_targets[0].target == newer);
}

TEST_CASE("radar reveal timers persist for retained targets and reset for new targets") {
  entt::registry registry;
  const entt::entity original = asteroid_at(registry, {.x = 180.0F, .y = 100.0F});
  hyperverse::RadarHudModel radar;

  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    {.max_targets = 2, .update_interval_seconds = 0.25F, .reveal_seconds = 0.5F, .range_world = 500.0F}
  );
  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.max_targets = 2, .update_interval_seconds = 0.25F, .reveal_seconds = 0.5F, .range_world = 500.0F}
  );
  (void)asteroid_at(registry, {.x = 120.0F, .y = 100.0F});
  hyperverse::update_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.max_targets = 2, .update_interval_seconds = 0.25F, .reveal_seconds = 0.5F, .range_world = 500.0F}
  );

  REQUIRE(radar.tracked_targets.size() == 2);
  const auto original_entry = std::ranges::find_if(radar.tracked_targets, [original](const hyperverse::RadarTrackedTarget& tracked) {
    return tracked.target == original;
  });
  REQUIRE(original_entry != radar.tracked_targets.end());
  CHECK(original_entry->reveal_seconds == Catch::Approx(0.5F));
  CHECK(radar.tracked_targets[0].reveal_seconds == Catch::Approx(0.0F));
}

TEST_CASE("combat radar tracks only active enemy ships") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(asteroid, hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}});
  const entt::entity dormant_thief = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    dormant_thief,
    hyperverse::RaiderShip{.position = {.x = 130.0F, .y = 100.0F}, .phase = hyperverse::RaiderPhase::Idle, .role = hyperverse::RaiderRole::CargoThief}
  );
  const entt::entity combat = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    combat,
    hyperverse::RaiderShip{.position = {.x = 160.0F, .y = 100.0F}, .role = hyperverse::RaiderRole::Combat}
  );

  hyperverse::CombatRadarHudModel radar;
  hyperverse::update_combat_radar_hud(
    radar,
    registry,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.max_targets = 4, .range_world = 500.0F}
  );

  REQUIRE(radar.tracked_targets.size() == 1U);
  CHECK(radar.tracked_targets.front().target == combat);
  REQUIRE(radar.target_order.size() == 1U);
  CHECK(radar.target_order.front() == combat);
}

TEST_CASE("asteroid and combat radar maintain separate target order lists") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(asteroid, hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}});
  const entt::entity combat = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    combat,
    hyperverse::RaiderShip{.position = {.x = 160.0F, .y = 100.0F}, .role = hyperverse::RaiderRole::Combat}
  );

  hyperverse::RadarHudModel asteroid_radar;
  hyperverse::CombatRadarHudModel combat_radar;
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::RadarHudTuning tuning{.max_targets = 4, .range_world = 500.0F};

  hyperverse::update_radar_hud(asteroid_radar, registry, ship, sector, 0.25F, tuning);
  hyperverse::update_combat_radar_hud(combat_radar, registry, ship, sector, 0.25F, tuning);

  REQUIRE(asteroid_radar.target_order.size() == 1U);
  REQUIRE(combat_radar.target_order.size() == 1U);
  CHECK(asteroid_radar.target_order.front() == asteroid);
  CHECK(combat_radar.target_order.front() == combat);
  CHECK(asteroid_radar.target_order.front() != combat_radar.target_order.front());
}
