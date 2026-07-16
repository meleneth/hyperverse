#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("raider acquires and approaches the rearmost cargo box during escort") {
  entt::registry registry;
  const entt::entity front = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    front,
    hyperverse::CargoBox{.position = {.x = 500.0F, .y = 100.0F}, .index = 0}
  );
  const entt::entity rear = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    rear,
    hyperverse::CargoBox{.position = {.x = 700.0F, .y = 100.0F}, .index = 2}
  );
  hyperverse::RaiderShip raider{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    raider,
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 0.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    1.0F,
    {.max_speed = 100.0F, .disruption_range = 50.0F}
  );

  CHECK(hud.active);
  CHECK(hud.phase == hyperverse::RaiderPhase::Approaching);
  CHECK(hud.target_box == rear);
  CHECK(raider.target_box == rear);
  CHECK(raider.position.x == Catch::Approx(200.0F));
}

TEST_CASE("raider disrupts cargo coupling when inside range") {
  entt::registry registry;
  const entt::entity box = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box,
    hyperverse::CargoBox{.position = {.x = 120.0F, .y = 100.0F}, .index = 0}
  );
  hyperverse::RaiderShip raider{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    raider,
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 0.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.disruption_range = 50.0F, .disruption_seconds = 0.5F}
  );

  CHECK(hud.phase == hyperverse::RaiderPhase::Disrupting);
  CHECK(hud.disruption_fraction == Catch::Approx(0.5F));
  CHECK(raider.disruption_seconds == Catch::Approx(0.25F));
}

TEST_CASE("raider steals and tows a cargo box after disruption completes") {
  entt::registry registry;
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 120.0F, .y = 100.0F}, .index = 0}
  );
  hyperverse::RaiderShip raider{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    raider,
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 0.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 100.0F, .disruption_range = 50.0F, .disruption_seconds = 0.5F}
  );

  const hyperverse::CargoBox& box = registry.get<hyperverse::CargoBox>(box_entity);
  CHECK(hud.phase == hyperverse::RaiderPhase::Towing);
  CHECK(hud.escape_distance == Catch::Approx(150.0F));
  CHECK(box.state == hyperverse::CargoBoxState::Stolen);
  CHECK(box.position.x == Catch::Approx(150.0F));
  CHECK(raider.position.x == Catch::Approx(150.0F));
}

TEST_CASE("stolen cargo recovery reports nearby boxes without relinking") {
  entt::registry registry;
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 140.0F, .y = 100.0F}, .index = 0, .state = hyperverse::CargoBoxState::Stolen}
  );
  hyperverse::RaiderShip raider{.target_box = box_entity, .phase = hyperverse::RaiderPhase::Towing};

  const hyperverse::CargoRecoveryHudSnapshot hud = hyperverse::recover_stolen_cargo(
    registry,
    raider,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {},
    {.width = 9000.0F, .height = 9000.0F},
    {.recovery_range = 80.0F}
  );

  CHECK(hud.stolen_box_near);
  CHECK_FALSE(hud.recovered);
  CHECK(hud.nearest_stolen_distance == Catch::Approx(40.0F));
  CHECK(registry.get<hyperverse::CargoBox>(box_entity).state == hyperverse::CargoBoxState::Stolen);
}

TEST_CASE("confirm near stolen cargo relinks it and drops the raider target") {
  entt::registry registry;
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 140.0F, .y = 100.0F}, .index = 0, .state = hyperverse::CargoBoxState::Stolen}
  );
  hyperverse::RaiderShip raider{.target_box = box_entity, .phase = hyperverse::RaiderPhase::Towing, .disruption_seconds = 0.5F};

  const hyperverse::CargoRecoveryHudSnapshot hud = hyperverse::recover_stolen_cargo(
    registry,
    raider,
    {.position = {.x = 100.0F, .y = 100.0F}, .velocity = {.x = 25.0F, .y = 0.0F}},
    {.confirm_requested = true},
    {.width = 9000.0F, .height = 9000.0F},
    {.recovery_range = 80.0F}
  );

  const hyperverse::CargoBox& box = registry.get<hyperverse::CargoBox>(box_entity);
  CHECK(hud.recovered);
  CHECK(box.state == hyperverse::CargoBoxState::Linked);
  CHECK(box.velocity.x == Catch::Approx(25.0F));
  CHECK((raider.target_box == entt::null));
  CHECK(raider.phase == hyperverse::RaiderPhase::Idle);
  CHECK(raider.disruption_seconds == Catch::Approx(0.0F));
}

TEST_CASE("raider escape marks stolen cargo as lost") {
  entt::registry registry;
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 100.0F, .y = 100.0F}, .index = 0, .state = hyperverse::CargoBoxState::Stolen}
  );
  hyperverse::RaiderShip raider{
    .position = {.x = 100.0F, .y = 100.0F},
    .target_box = box_entity,
    .phase = hyperverse::RaiderPhase::Towing,
  };

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    raider,
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 0.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    1.0F,
    {.max_speed = 100.0F, .escape_distance = 200.0F}
  );

  CHECK(hud.phase == hyperverse::RaiderPhase::Escaped);
  CHECK(hud.escape_distance == Catch::Approx(200.0F));
  CHECK(registry.get<hyperverse::CargoBox>(box_entity).state == hyperverse::CargoBoxState::Lost);
  CHECK((raider.target_box == entt::null));
}
