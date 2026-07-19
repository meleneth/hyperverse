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
  CHECK(hud.task == hyperverse::RaiderTask::StealCargo);
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

TEST_CASE("confirm near stolen cargo emits cargo and raider FSM events") {
  entt::registry registry;
  const entt::entity raider_entity = registry.create();
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 140.0F, .y = 100.0F}, .index = 0, .state = hyperverse::CargoBoxState::Stolen}
  );
  hyperverse::RaiderShip raider{.target_box = box_entity, .phase = hyperverse::RaiderPhase::Towing};
  hyperverse::DomainEventBus event_bus;
  int box_state_events = 0;
  int raider_phase_events = 0;
  event_bus.appendListener(
    hyperverse::DomainEventType::CargoBoxStateChanged,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.subject == box_entity);
      CHECK(event.count == static_cast<int>(hyperverse::CargoBoxState::Linked));
      ++box_state_events;
    }
  );
  event_bus.appendListener(
    hyperverse::DomainEventType::RaiderPhaseChanged,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.subject == raider_entity);
      CHECK(event.count == static_cast<int>(hyperverse::RaiderPhase::Idle));
      ++raider_phase_events;
    }
  );

  const hyperverse::CargoRecoveryHudSnapshot hud = hyperverse::recover_stolen_cargo(
    registry,
    raider,
    {.position = {.x = 100.0F, .y = 100.0F}},
    {.confirm_requested = true},
    {.width = 9000.0F, .height = 9000.0F},
    {.recovery_range = 80.0F},
    raider_entity,
    &event_bus
  );
  event_bus.process();

  CHECK(hud.recovered);
  CHECK(box_state_events == 1);
  CHECK(raider_phase_events == 1);
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

TEST_CASE("raider phase FSM rejects invalid transitions and emits phase changes") {
  hyperverse::RaiderShip raider{.position = {.x = 10.0F, .y = 20.0F}};
  hyperverse::DomainEventBus event_bus;
  int phase_events = 0;
  event_bus.appendListener(
    hyperverse::DomainEventType::RaiderPhaseChanged,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.subject == entt::entity{99});
      CHECK(event.position.x == Catch::Approx(10.0F));
      ++phase_events;
    }
  );

  CHECK_FALSE(hyperverse::transition_raider_phase(raider, hyperverse::RaiderTransition::Escape, entt::entity{99}, &event_bus));
  CHECK(raider.phase == hyperverse::RaiderPhase::Idle);
  CHECK(hyperverse::transition_raider_phase(raider, hyperverse::RaiderTransition::Approach, entt::entity{99}, &event_bus));
  CHECK(raider.phase == hyperverse::RaiderPhase::Approaching);
  CHECK(hyperverse::transition_raider_phase(raider, hyperverse::RaiderTransition::BeginDisruption, entt::entity{99}, &event_bus));
  CHECK(raider.phase == hyperverse::RaiderPhase::Disrupting);
  event_bus.process();

  CHECK(phase_events == 2);
}

TEST_CASE("raider task FSM emits only real task changes") {
  hyperverse::RaiderShip raider{.task = hyperverse::RaiderTask::HarassPlayer};
  hyperverse::DomainEventBus event_bus;
  int task_events = 0;
  event_bus.appendListener(
    hyperverse::DomainEventType::RaiderTaskChanged,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.subject == entt::entity{7});
      CHECK(event.count == static_cast<int>(hyperverse::RaiderTask::CoverThief));
      ++task_events;
    }
  );

  CHECK_FALSE(hyperverse::transition_raider_task(raider, hyperverse::RaiderTaskTransition::HarassPlayer, entt::entity{7}, &event_bus));
  CHECK(hyperverse::transition_raider_task(raider, hyperverse::RaiderTaskTransition::CoverThief, entt::entity{7}, &event_bus));
  event_bus.process();

  CHECK(raider.task == hyperverse::RaiderTask::CoverThief);
  CHECK(task_events == 1);
}

TEST_CASE("gate combat raiders spawn as player attackers with particle cannons") {
  entt::registry registry;
  hyperverse::spawn_gate_combat_raiders(
    registry,
    {.x = 1000.0F, .y = 1000.0F},
    {.x = 800.0F, .y = 1000.0F},
    {.width = 9000.0F, .height = 9000.0F},
    3
  );

  int combat_raiders = 0;
  for (auto [entity, raider] : registry.view<hyperverse::RaiderShip>().each()) {
    CHECK(raider.role == hyperverse::RaiderRole::Combat);
    CHECK(raider.task == hyperverse::RaiderTask::FullAggression);
    CHECK(registry.all_of<hyperverse::ParticleCannonModel>(entity));
    ++combat_raiders;
  }

  CHECK(combat_raiders == 3);
}

TEST_CASE("pressure escalation spawns harassment raiders around the player") {
  entt::registry registry;

  const int spawned = hyperverse::spawn_pressure_raiders(
    registry,
    {.x = 4500.0F, .y = 4500.0F},
    {.width = 9000.0F, .height = 9000.0F},
    2
  );

  CHECK(spawned == 2);
  int raiders = 0;
  for (auto [entity, raider] : registry.view<hyperverse::RaiderShip>().each()) {
    CHECK(registry.all_of<hyperverse::ParticleCannonModel>(entity));
    CHECK(raider.role == hyperverse::RaiderRole::Combat);
    CHECK(raider.task == hyperverse::RaiderTask::HarassPlayer);
    CHECK(raider.integrity == Catch::Approx(82.0F));
    ++raiders;
  }
  CHECK(raiders == spawned);
}

TEST_CASE("high pressure spawns full aggression raiders") {
  entt::registry registry;

  const int spawned = hyperverse::spawn_pressure_raiders(
    registry,
    {.x = 4500.0F, .y = 4500.0F},
    {.width = 9000.0F, .height = 9000.0F},
    6
  );

  CHECK(spawned == 4);
  for (auto [entity, raider] : registry.view<hyperverse::RaiderShip>().each()) {
    (void)entity;
    CHECK(raider.task == hyperverse::RaiderTask::FullAggression);
    CHECK(raider.integrity == Catch::Approx(106.0F));
  }
}

TEST_CASE("combat raiders buzz the player in moving oval attack runs") {
  entt::registry registry;
  hyperverse::RaiderShip combat{
    .position = {.x = 1000.0F, .y = 1000.0F},
    .role = hyperverse::RaiderRole::Combat,
    .orbit_radians = 0.0F,
  };

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    combat,
    registry,
    {.phase = hyperverse::CargoEscortPhase::Mining},
    {.position = {.x = 1000.0F, .y = 1000.0F}, .facing_radians = 0.0F},
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 500.0F, .combat_orbit_x_radius = 1200.0F, .combat_orbit_y_radius = 600.0F, .combat_orbit_radians_per_second = 1.0F}
  );

  CHECK(hud.active);
  CHECK(hud.task == hyperverse::RaiderTask::HarassPlayer);
  CHECK(hyperverse::length(combat.velocity) > 0.0F);
  CHECK(combat.position.x != Catch::Approx(1000.0F));
  CHECK(combat.position.y != Catch::Approx(1000.0F));
  CHECK(combat.orbit_radians == Catch::Approx(0.5F));
}

TEST_CASE("combat raider movement is acceleration constrained") {
  entt::registry registry;
  hyperverse::RaiderShip combat{
    .position = {.x = 1000.0F, .y = 1000.0F},
    .role = hyperverse::RaiderRole::Combat,
  };

  (void)hyperverse::update_raider_threat(
    combat,
    registry,
    {.phase = hyperverse::CargoEscortPhase::Mining},
    {.position = {.x = 1000.0F, .y = 1000.0F}, .facing_radians = 0.0F},
    {.width = 9000.0F, .height = 9000.0F},
    0.1F,
    {.max_speed = 500.0F, .combat_acceleration = 100.0F}
  );

  CHECK(hyperverse::length(combat.velocity) == Catch::Approx(10.0F));
  CHECK(hyperverse::length(combat.velocity) < 500.0F);
}

TEST_CASE("active raiders fade in from cloak") {
  entt::registry registry;
  hyperverse::RaiderShip combat{
    .position = {.x = 1000.0F, .y = 1000.0F},
    .role = hyperverse::RaiderRole::Combat,
  };

  (void)hyperverse::update_raider_threat(
    combat,
    registry,
    {.phase = hyperverse::CargoEscortPhase::Mining},
    {.position = {.x = 1000.0F, .y = 1000.0F}, .facing_radians = 0.0F},
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.cloak_fade_seconds = 1.0F}
  );
  CHECK(combat.cloak_fade_seconds == Catch::Approx(0.25F));

  (void)hyperverse::update_raider_threat(
    combat,
    registry,
    {.phase = hyperverse::CargoEscortPhase::Mining},
    {.position = {.x = 1000.0F, .y = 1000.0F}, .facing_radians = 0.0F},
    {.width = 9000.0F, .height = 9000.0F},
    2.0F,
    {.cloak_fade_seconds = 1.0F}
  );
  CHECK(combat.cloak_fade_seconds == Catch::Approx(1.0F));
}

TEST_CASE("combat raiders switch to cover when a thief is stealing cargo") {
  entt::registry registry;
  const entt::entity thief_entity = registry.create();
  registry.emplace<hyperverse::RaiderShip>(
    thief_entity,
    hyperverse::RaiderShip{.phase = hyperverse::RaiderPhase::Towing, .role = hyperverse::RaiderRole::CargoThief}
  );
  hyperverse::RaiderShip combat{
    .position = {.x = 100.0F, .y = 100.0F},
    .role = hyperverse::RaiderRole::Combat,
  };

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    combat,
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 400.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F
  );

  CHECK(hud.task == hyperverse::RaiderTask::CoverThief);
  CHECK(combat.task == hyperverse::RaiderTask::CoverThief);
}

TEST_CASE("combat raiders switch to full aggression during extraction") {
  entt::registry registry;
  hyperverse::RaiderShip combat{
    .position = {.x = 100.0F, .y = 100.0F},
    .role = hyperverse::RaiderRole::Combat,
  };

  const hyperverse::RaiderHudSnapshot hud = hyperverse::update_raider_threat(
    combat,
    registry,
    {.phase = hyperverse::CargoEscortPhase::Extracting},
    {.position = {.x = 400.0F, .y = 100.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    0.0F
  );

  CHECK(hud.task == hyperverse::RaiderTask::FullAggression);
  CHECK(combat.task == hyperverse::RaiderTask::FullAggression);
}
