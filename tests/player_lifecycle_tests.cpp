#include "test_common.hpp"
#include "hyperverse/player_lifecycle.hpp"

TEST_CASE("player death and timed respawn are FSM transitions driven by domain events") {
  hyperverse::test::TestAccountWorld world;
  const entt::entity player = world.registry.create();
  world.registry.emplace<hyperverse::ShipMotion>(player, hyperverse::ShipMotion{.position = {40.0F, 50.0F}});
  world.registry.emplace<hyperverse::ShipHealth>(player, hyperverse::ShipHealth{.armor = 0.0F, .shields = 0.0F});
  const entt::entity drone = world.registry.create();
  world.registry.emplace<hyperverse::MiningDrone>(drone);
  world.registry.emplace<hyperverse::DronePresence>(drone, hyperverse::DronePresence{.phase = hyperverse::DronePresencePhase::Following});
  const std::vector<entt::entity> drones{drone};
  hyperverse::PlayerLifecycleModel lifecycle{.home_position = {100.0F, 200.0F}};
  hyperverse::install_drone_presence_event_handlers(world.registry, drones, world.event_bus);
  hyperverse::install_player_lifecycle_event_handlers(lifecycle, world.registry, player, drones, world.event_bus);

  hyperverse::update_player_lifecycle(lifecycle, world.registry, player, 0.0F, world.event_bus);
  world.event_bus.process();
  CHECK(lifecycle.phase == hyperverse::PlayerLifecyclePhase::AwaitingRespawn);
  CHECK(lifecycle.respawn_seconds_remaining == Catch::Approx(15.0F));
  CHECK(world.registry.get<hyperverse::DronePresence>(drone).phase == hyperverse::DronePresencePhase::ExitBarrelRoll);

  hyperverse::update_drone_presence(world.registry, drones, 0.8F, world.event_bus);
  world.event_bus.process();
  CHECK(world.registry.get<hyperverse::DronePresence>(drone).phase == hyperverse::DronePresencePhase::Hidden);

  hyperverse::update_player_lifecycle(lifecycle, world.registry, player, 14.9F, world.event_bus);
  world.event_bus.process();
  CHECK(lifecycle.phase == hyperverse::PlayerLifecyclePhase::AwaitingRespawn);
  hyperverse::update_player_lifecycle(lifecycle, world.registry, player, 0.1F, world.event_bus);
  world.event_bus.process();
  CHECK(lifecycle.phase == hyperverse::PlayerLifecyclePhase::Alive);
  CHECK(world.registry.get<hyperverse::ShipMotion>(player).position.x == Catch::Approx(100.0F));
  CHECK(world.registry.get<hyperverse::ShipHealth>(player).armor == Catch::Approx(100.0F));
  CHECK(world.registry.get<hyperverse::DronePresence>(drone).phase == hyperverse::DronePresencePhase::SpawnBarrelRoll);
}

TEST_CASE("initially spawned drone rolls before it follows") {
  hyperverse::test::TestAccountWorld world;
  const entt::entity drone = world.registry.create();
  world.registry.emplace<hyperverse::MiningDrone>(drone);
  world.registry.emplace<hyperverse::DronePresence>(drone, hyperverse::DronePresence{.phase_seconds_remaining = 0.8F});
  const std::vector<entt::entity> drones{drone};
  hyperverse::install_drone_presence_event_handlers(world.registry, drones, world.event_bus);
  hyperverse::update_drone_presence(world.registry, drones, 0.4F, world.event_bus);
  world.event_bus.process();
  CHECK(world.registry.get<hyperverse::DronePresence>(drone).phase == hyperverse::DronePresencePhase::SpawnBarrelRoll);
  CHECK(world.registry.get<hyperverse::DronePresence>(drone).roll_radians == Catch::Approx(3.1415926F));
  hyperverse::update_drone_presence(world.registry, drones, 0.4F, world.event_bus);
  world.event_bus.process();
  CHECK(world.registry.get<hyperverse::DronePresence>(drone).phase == hyperverse::DronePresencePhase::Following);
}
