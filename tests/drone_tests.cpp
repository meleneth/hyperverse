#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("mining drone work phase transitions through SML owner") {
  hyperverse::MiningDrone drone{};

  CHECK(hyperverse::transition_mining_drone_work(drone, hyperverse::MiningDroneWorkTransition::TravelToWork));
  CHECK(drone.phase == hyperverse::MiningDronePhase::Travelling);
  CHECK(hyperverse::transition_mining_drone_work(drone, hyperverse::MiningDroneWorkTransition::BeginMining));
  CHECK(drone.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(hyperverse::transition_mining_drone_work(drone, hyperverse::MiningDroneWorkTransition::ReturnToFormation));
  CHECK(drone.phase == hyperverse::MiningDronePhase::Idle);
}

TEST_CASE("mining drone cargo phase transitions through SML owner") {
  hyperverse::MiningDrone drone{.cargo_target = entt::entity{7}};

  CHECK(hyperverse::transition_mining_drone_cargo(drone, hyperverse::MiningDroneCargoTransition::AssignCargo));
  CHECK(drone.phase == hyperverse::MiningDronePhase::CargoPickup);
  CHECK(hyperverse::transition_mining_drone_cargo(drone, hyperverse::MiningDroneCargoTransition::CargoPickedUp));
  CHECK(drone.phase == hyperverse::MiningDronePhase::EscortingCargo);
  CHECK(hyperverse::transition_mining_drone_cargo(drone, hyperverse::MiningDroneCargoTransition::CargoDelivered));
  CHECK(drone.phase == hyperverse::MiningDronePhase::Idle);
}

TEST_CASE("mining drone acquires the locked asteroid as its priority") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone drone{.position = {.x = 100.0F, .y = 100.0F}};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 100.0F, .mining_range = 50.0F, .work_standoff = 40.0F, .work_angle_rotation_radians_per_second = 0.0F}
  );

  CHECK(drone.target == asteroid);
  CHECK(drone.phase == hyperverse::MiningDronePhase::Travelling);
  CHECK(drone.position.x == Catch::Approx(150.0F));
  CHECK(hud.target == asteroid);
}

TEST_CASE("mining drone extracts material when in range") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 120.0F, .y = 100.0F}, .radius = 40.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone drone{.position = {.x = 200.0F, .y = 100.0F}, .target = asteroid};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    2.0F,
    {.mining_range = 50.0F, .work_standoff = 40.0F, .arrival_tolerance = 50.0F, .integrity_damage_per_second = 4.0F, .extraction_per_second = 3.0F}
  );

  const hyperverse::MiningResource& resource = registry.get<hyperverse::MiningResource>(asteroid);
  CHECK(drone.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(hud.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(resource.integrity == Catch::Approx(92.0F));
  CHECK(resource.extracted_mass == Catch::Approx(6.0F));
  CHECK(drone.extracted_mass == Catch::Approx(6.0F));
}

TEST_CASE("mining drones ignore largest asteroid break tier") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 100.0F}, .radius = 480.0F}
  );
  registry.emplace<hyperverse::AsteroidFragmentation>(asteroid, hyperverse::AsteroidFragmentation{.remaining_breaks = 2});
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone drone{.position = {.x = 100.0F, .y = 100.0F}};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 100.0F}
  );

  CHECK_FALSE(registry.valid(drone.target));
  CHECK_FALSE(registry.valid(hud.target));
  CHECK(drone.phase == hyperverse::MiningDronePhase::Idle);
}

TEST_CASE("mining drones use separate work angles around an asteroid") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 500.0F, .y = 500.0F}, .radius = 80.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone first{.position = {.x = 500.0F, .y = 500.0F}, .target = asteroid, .work_angle_radians = 0.0F};
  hyperverse::MiningDrone second{.position = {.x = 500.0F, .y = 500.0F}, .target = asteroid, .work_angle_radians = 3.1415926F};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  (void)hyperverse::update_mining_drone(first, registry, {}, ship, {.width = 9000.0F, .height = 9000.0F}, 0.5F, {.max_speed = 100.0F, .work_standoff = 120.0F});
  (void)hyperverse::update_mining_drone(second, registry, {}, ship, {.width = 9000.0F, .height = 9000.0F}, 0.5F, {.max_speed = 100.0F, .work_standoff = 120.0F});

  CHECK(first.velocity.x > 0.0F);
  CHECK(second.velocity.x < 0.0F);
}

TEST_CASE("idle mining drones form up behind the player ship") {
  entt::registry registry;
  hyperverse::MiningDrone drone{.position = {.x = 500.0F, .y = 500.0F}, .work_angle_radians = 0.0F};
  const hyperverse::ShipMotion ship{.position = {.x = 500.0F, .y = 500.0F}, .facing_radians = 0.0F};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.max_speed = 100.0F, .formation_trail_distance = 260.0F, .formation_spread = 0.0F}
  );

  CHECK(drone.phase == hyperverse::MiningDronePhase::Idle);
  CHECK(drone.velocity.x < 0.0F);
  CHECK(drone.position.x < 500.0F);
  CHECK_FALSE(registry.valid(hud.target));
  CHECK(hud.target_distance > 0.0F);
}

TEST_CASE("mining drone work angles rotate slowly over time") {
  entt::registry registry;
  hyperverse::MiningDrone drone{.position = {.x = 500.0F, .y = 500.0F}, .work_angle_radians = 6.20F};
  const hyperverse::ShipMotion ship{.position = {.x = 500.0F, .y = 500.0F}};

  (void)hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    1.0F,
    {.max_speed = 100.0F, .work_angle_rotation_radians_per_second = 0.20F}
  );

  CHECK(drone.work_angle_radians == Catch::Approx(0.1168146F));
}

TEST_CASE("mining drone releases targets that move beyond operating range") {
  entt::registry registry;
  hyperverse::DomainEventBus event_bus;
  int release_events = 0;
  event_bus.appendListener(
    hyperverse::DomainEventType::DroneTargetReleased,
    [&](const hyperverse::DomainEvent& event) {
      CHECK(event.type == hyperverse::DomainEventType::DroneTargetReleased);
      ++release_events;
    }
  );
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 2000.0F, .y = 100.0F}, .radius = 80.0F}
  );
  registry.emplace<hyperverse::MiningResource>(asteroid);
  hyperverse::MiningDrone drone{.position = {.x = 100.0F, .y = 100.0F}, .target = asteroid};
  const hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.25F,
    {.max_speed = 100.0F, .max_target_distance_from_ship = 500.0F},
    &event_bus
  );
  event_bus.process();

  CHECK_FALSE(registry.valid(drone.target));
  CHECK(drone.phase == hyperverse::MiningDronePhase::Idle);
  CHECK_FALSE(registry.valid(hud.target));
  CHECK(release_events == 1);
}

TEST_CASE("mining drone picks up one pending cargo box and delivers it to the gathering ship") {
  entt::registry registry;
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 140.0F, .y = 100.0F}, .index = 0, .state = hyperverse::CargoBoxState::PendingPickup}
  );
  hyperverse::MiningDrone drone{.position = {.x = 100.0F, .y = 100.0F}};
  const hyperverse::ShipMotion ship{.position = {.x = 300.0F, .y = 100.0F}};
  drone.cargo_target = box_entity;
  drone.cargo_destination = ship.position;
  hyperverse::DomainEventBus event_bus;
  int pickup_events = 0;
  int delivered_events = 0;
  event_bus.appendListener(hyperverse::DomainEventType::CargoBoxPickupStarted, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.target == box_entity);
    ++pickup_events;
  });
  event_bus.appendListener(hyperverse::DomainEventType::CargoBoxDeliveredToGathering, [&](const hyperverse::DomainEvent& event) {
    CHECK(event.target == box_entity);
    CHECK(event.position.x == Catch::Approx(ship.position.x));
    CHECK(event.position.y == Catch::Approx(ship.position.y));
    ++delivered_events;
  });

  const hyperverse::MiningDroneHudSnapshot pickup = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 80.0F, .cargo_pickup_tolerance = 45.0F},
    &event_bus
  );
  event_bus.process();

  CHECK(pickup.phase == hyperverse::MiningDronePhase::CargoPickup);
  CHECK(pickup.target == box_entity);
  CHECK(registry.get<hyperverse::CargoBox>(box_entity).state == hyperverse::CargoBoxState::BeingHauled);
  CHECK(pickup_events == 1);

  (void)hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    1.95F,
    {.max_speed = 80.0F, .cargo_pickup_tolerance = 45.0F, .cargo_delivery_tolerance = 45.0F},
    &event_bus
  );
  const hyperverse::MiningDroneHudSnapshot delivery = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    ship,
    {.width = 9000.0F, .height = 9000.0F},
    0.1F,
    {.max_speed = 80.0F, .cargo_pickup_tolerance = 45.0F, .cargo_delivery_tolerance = 45.0F},
    &event_bus
  );
  event_bus.process();

  const hyperverse::CargoBox& box = registry.get<hyperverse::CargoBox>(box_entity);
  CHECK(delivery.phase == hyperverse::MiningDronePhase::EscortingCargo);
  CHECK(box.state == hyperverse::CargoBoxState::Linked);
  CHECK(hyperverse::length(hyperverse::wrapped_delta(box.position, ship.position, {.width = 9000.0F, .height = 9000.0F})) <= 45.0F);
  CHECK_FALSE(registry.valid(drone.cargo_target));
  CHECK(delivered_events == 1);
}
