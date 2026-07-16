#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("mining laser extracts material from locked asteroid in range") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  const hyperverse::TargetLockModel lock{
    .phase = hyperverse::TargetLockPhase::Locked,
    .target = asteroid,
    .wrapped_distance = 250.0F,
  };
  const hyperverse::MiningLaserTuning tuning{
    .range = 500.0F,
    .integrity_damage_per_second = 20.0F,
    .extraction_per_second = 10.0F,
    .heat_per_second = 30.0F,
    .heat_decay_per_second = 5.0F,
  };

  const hyperverse::MiningHudSnapshot active =
    hyperverse::update_mining_laser(
      registry,
      lock,
      {.position = {.x = 100.0F, .y = 100.0F}},
      {.tool_intensity = 0.5F},
      {.width = 9000.0F, .height = 9000.0F},
      tuning,
      2.0F
    );

  CHECK(active.beam_active);
  CHECK(active.target_in_range);
  CHECK(active.target == asteroid);
  CHECK(active.target_integrity == Catch::Approx(80.0F));
  CHECK(active.target_heat == Catch::Approx(30.0F));
  CHECK(active.extracted_mass == Catch::Approx(10.0F));

  const hyperverse::MiningHudSnapshot cooling = hyperverse::update_mining_laser(
    registry,
    lock,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {},
    {.width = 9000.0F, .height = 9000.0F},
    tuning,
    2.0F
  );
  CHECK_FALSE(cooling.beam_active);
  CHECK(cooling.target_heat == Catch::Approx(20.0F));
}

TEST_CASE("mining laser reduces asteroid collision mass as integrity drops") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 80.0F, .base_radius = 80.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  (void)hyperverse::update_mining_laser(
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid, .wrapped_distance = 250.0F},
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.tool_intensity = 1.0F},
    {.width = 9000.0F, .height = 9000.0F},
    {.range = 500.0F, .integrity_damage_per_second = 25.0F},
    2.0F
  );

  CHECK(registry.get<hyperverse::AsteroidBody>(asteroid).radius == Catch::Approx(40.0F));
}

TEST_CASE("mining depletion keeps asteroids at one sixth of base radius") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 600.0F, .base_radius = 600.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  (void)hyperverse::update_mining_laser(
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid, .wrapped_distance = 250.0F},
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.tool_intensity = 1.0F},
    {.width = 9000.0F, .height = 9000.0F},
    {.range = 500.0F, .integrity_damage_per_second = 90.0F},
    1.0F
  );

  CHECK(registry.get<hyperverse::AsteroidBody>(asteroid).radius == Catch::Approx(100.0F));
}

TEST_CASE("mining depletion breaks an asteroid into laser-coherent fragments") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 600.0F, .base_radius = 600.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  const hyperverse::MiningHudSnapshot hud = hyperverse::update_mining_laser(
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid, .wrapped_distance = 250.0F},
    {.position = {.x = 100.0F, .y = 100.0F}, .facing_radians = 0.0F},
    {.tool_intensity = 1.0F},
    {.width = 9000.0F, .height = 9000.0F},
    {.range = 500.0F, .integrity_damage_per_second = 120.0F},
    1.0F
  );

  CHECK(hud.target_integrity == Catch::Approx(0.0F));
  CHECK_FALSE(registry.valid(asteroid));
  CHECK(std::distance(registry.view<hyperverse::AsteroidBody>().begin(), registry.view<hyperverse::AsteroidBody>().end()) == 4);
}

TEST_CASE("ore tiers expose distinct asteroid tint colors") {
  const hyperverse::OreTint common = hyperverse::ore_tint(hyperverse::OreTier::Common);
  const hyperverse::OreTint rare = hyperverse::ore_tint(hyperverse::OreTier::Rare);
  const hyperverse::OreTint exotic = hyperverse::ore_tint(hyperverse::OreTier::Exotic);
  const hyperverse::OreTint anomalous = hyperverse::ore_tint(hyperverse::OreTier::Anomalous);

  CHECK(rare.b > common.b);
  CHECK(exotic.r == Catch::Approx(1.0F));
  CHECK(exotic.g < common.g);
  CHECK(anomalous.g == Catch::Approx(1.0F));
}

TEST_CASE("mining laser can acquire an asteroid from aim without a target lock") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  const hyperverse::MiningHudSnapshot active = hyperverse::update_mining_laser(
    registry,
    {},
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.primary_aim = {.x = 1.0F, .y = 0.0F}, .tool_intensity = 1.0F},
    {.width = 9000.0F, .height = 9000.0F},
    {.range = 500.0F, .integrity_damage_per_second = 25.0F},
    1.0F
  );

  CHECK(active.beam_active);
  CHECK(active.target == asteroid);
  CHECK(active.beam_end_position.x == Catch::Approx(350.0F));
  CHECK(active.beam_end_position.y == Catch::Approx(100.0F));
  CHECK(registry.get<hyperverse::MiningResource>(asteroid).integrity == Catch::Approx(75.0F));
}

TEST_CASE("moderate mining extracts material without destabilizing the asteroid") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  const hyperverse::MiningHudSnapshot active = hyperverse::update_mining_laser(
    registry,
    {},
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.primary_aim = {.x = 1.0F, .y = 0.0F}, .tool_intensity = 0.35F},
    {.width = 9000.0F, .height = 9000.0F},
    {
      .range = 500.0F,
      .integrity_damage_per_second = 10.0F,
      .extraction_per_second = 12.0F,
      .heat_per_second = 20.0F,
      .stress_per_second = 15.0F,
      .pressure_per_second = 10.0F,
    },
    1.0F
  );

  CHECK(active.beam_active);
  CHECK_FALSE(active.unstable);
  CHECK_FALSE(active.blowout);
  CHECK(active.extracted_mass == Catch::Approx(4.2F));
}

TEST_CASE("reckless mining can trigger a volatile asteroid blowout") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 350.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  const hyperverse::MiningHudSnapshot blowout = hyperverse::update_mining_laser(
    registry,
    {},
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.primary_aim = {.x = 1.0F, .y = 0.0F}, .tool_intensity = 1.0F},
    {.width = 9000.0F, .height = 9000.0F},
    {
      .range = 500.0F,
      .integrity_damage_per_second = 0.0F,
      .extraction_per_second = 4.0F,
      .heat_per_second = 100.0F,
      .stress_per_second = 100.0F,
      .pressure_per_second = 100.0F,
      .unstable_heat = 70.0F,
      .unstable_stress = 65.0F,
      .volatile_pressure_limit = 55.0F,
      .blowout_integrity_damage = 30.0F,
    },
    1.0F
  );

  const hyperverse::MiningResource& resource = registry.get<hyperverse::MiningResource>(asteroid);
  CHECK(blowout.blowout);
  CHECK(blowout.unstable);
  CHECK(blowout.gas_venting);
  CHECK(resource.integrity == Catch::Approx(70.0F));
  CHECK(resource.volatile_pressure == Catch::Approx(0.0F));
  CHECK(resource.structural_stress == Catch::Approx(35.0F));
}

TEST_CASE("mining laser cannot hit targets outside range") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 1000.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);

  const hyperverse::TargetLockModel lock{
    .phase = hyperverse::TargetLockPhase::Locked,
    .target = asteroid,
    .wrapped_distance = 900.0F,
  };

  const hyperverse::MiningHudSnapshot snapshot =
    hyperverse::update_mining_laser(
      registry,
      lock,
      {.position = {.x = 100.0F, .y = 100.0F}},
      {.tool_intensity = 1.0F},
      {.width = 9000.0F, .height = 9000.0F},
      {.range = 500.0F},
      1.0F
    );

  CHECK_FALSE(snapshot.beam_active);
  CHECK_FALSE(snapshot.target_in_range);
  CHECK(snapshot.target_integrity == Catch::Approx(100.0F));
}
