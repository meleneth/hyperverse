#include "hyperverse/version.hpp"

#include <boost/sml.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <entt/entity/registry.hpp>
#include <eventpp/eventqueue.h>

#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/grand_central.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <sstream>
#include <string>

TEST_CASE("project metadata is available") {
  CHECK(hyperverse::application_name() == "Hyperverse");
  CHECK(hyperverse::version() == "0.1.0");
}

TEST_CASE("baseline dependencies are visible to project code") {
  entt::registry registry;
  const entt::entity entity = registry.create();
  CHECK((entity != entt::null));

  eventpp::EventQueue<int, void(const std::string&)> queue;
  bool received = false;
  queue.appendListener(1, [&](const std::string& value) { received = value == "ready"; });
  queue.enqueue(1, "ready");
  queue.process();
  CHECK(received);

  namespace sml = boost::sml;
  CHECK(std::string{sml::aux::get_type_name<int>()}.find("int") != std::string::npos);
}

TEST_CASE("flight input maps raw devices into semantic movement intent") {
  const hyperverse::SemanticInputFrame idle = hyperverse::map_flight_intent({.movement_axis = {.x = 0.05F, .y = 0.0F}});
  CHECK(idle.desired_movement.x == Catch::Approx(0.0F));
  CHECK(idle.desired_movement.y == Catch::Approx(0.0F));

  const hyperverse::SemanticInputFrame moving =
    hyperverse::map_flight_intent({
      .movement_axis = {.x = 1.0F, .y = 1.0F},
      .confirm = true,
      .target_cycle = true,
      .control_mapping = hyperverse::ControlMapping::Gamepad,
    });
  CHECK(hyperverse::length(moving.desired_movement) == Catch::Approx(1.0F));
  CHECK(moving.confirm_requested);
  CHECK(moving.target_cycle_requested);
  CHECK(moving.control_mapping == hyperverse::ControlMapping::Gamepad);
}

TEST_CASE("wrapped sector distance uses the shortest edge crossing") {
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::Vec2 from{.x = 8950.0F, .y = 100.0F};
  const hyperverse::Vec2 to{.x = 25.0F, .y = 100.0F};

  CHECK(hyperverse::wrapped_delta(from, to, sector).x == Catch::Approx(75.0F));
  CHECK(hyperverse::wrapped_distance(from, to, sector) == Catch::Approx(75.0F));
  CHECK(hyperverse::wrap_position({.x = -10.0F, .y = 9010.0F}, sector).x == Catch::Approx(8990.0F));
  CHECK(hyperverse::wrap_position({.x = -10.0F, .y = 9010.0F}, sector).y == Catch::Approx(10.0F));
}

TEST_CASE("assisted flight accelerates, brakes, and wraps through the sector") {
  hyperverse::ShipMotion ship{.position = {.x = 8995.0F, .y = 4500.0F}};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F, .acceleration = 100.0F, .braking = 200.0F, .turn_rate = 20.0F};

  hyperverse::simulate_assisted_flight(
    ship,
    {.desired_movement = {.x = 1.0F, .y = 0.0F}},
    flight,
    sector,
    1.0F
  );

  CHECK(ship.velocity.x == Catch::Approx(100.0F));
  CHECK(ship.position.x == Catch::Approx(95.0F));

  hyperverse::simulate_assisted_flight(ship, {}, flight, sector, 1.0F);
  CHECK(hyperverse::length(ship.velocity) == Catch::Approx(0.0F));
}

TEST_CASE("fixed timestep consumes deterministic simulation ticks") {
  hyperverse::FixedTimestep timestep{0.25F};
  timestep.accumulate(0.74F);

  CHECK(timestep.consume_tick());
  CHECK(timestep.consume_tick());
  CHECK_FALSE(timestep.consume_tick());
  CHECK(timestep.alpha() == Catch::Approx(0.96F));
}

TEST_CASE("flight HUD exposes speed load, wrap warning, and control mapping") {
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F};
  const hyperverse::FlightHudTuning hud{.wrap_warning_distance = 500.0F};
  const hyperverse::ShipMotion ship{
    .position = {.x = 8750.0F, .y = 4400.0F},
    .velocity = {.x = 75.0F, .y = 0.0F},
  };
  const hyperverse::SemanticInputFrame input{
    .desired_movement = {.x = 1.0F, .y = 0.0F},
    .control_mapping = hyperverse::ControlMapping::Gamepad,
  };

  const hyperverse::FlightHudSnapshot snapshot = hyperverse::make_flight_hud_snapshot(ship, input, flight, sector, hud);

  CHECK(snapshot.speed == Catch::Approx(75.0F));
  CHECK(snapshot.speed_fraction == Catch::Approx(0.75F));
  CHECK(snapshot.nearest_wrap_edge_distance == Catch::Approx(250.0F));
  CHECK(snapshot.wrap_warning);
  CHECK(snapshot.control_mapping == hyperverse::ControlMapping::Gamepad);
}

TEST_CASE("camera anchor lags across wrapped sector edges") {
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::CameraTuning tuning{
    .position_lag_seconds = 1.0F,
    .rotation_lag_seconds = 1.0F,
    .velocity_lookahead_seconds = 0.0F,
  };
  hyperverse::CameraState camera{.position = {.x = 8950.0F, .y = 4500.0F}};
  const hyperverse::ShipMotion ship{
    .position = {.x = 50.0F, .y = 4500.0F},
    .facing_radians = 1.0F,
  };

  hyperverse::update_camera_anchor(camera, ship, sector, tuning, 1.0F);

  CHECK(camera.position.x < 50.0F);
  CHECK(camera.position.y == Catch::Approx(4500.0F));
  CHECK(camera.rotation_radians > 0.0F);
  CHECK(camera.rotation_radians < ship.facing_radians);
}

TEST_CASE("target lock acquires nearest asteroid and reports wrapped distance") {
  entt::registry registry;
  const entt::entity far = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(far, hyperverse::AsteroidBody{.position = {.x = 4000.0F, .y = 4500.0F}});
  const entt::entity near = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    near,
    hyperverse::AsteroidBody{.position = {.x = 25.0F, .y = 4500.0F}, .scan_confidence = 0.62F}
  );

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 700.0F};

  hyperverse::update_target_lock(
    lock,
    registry,
    {.x = 8950.0F, .y = 4500.0F},
    {.target_cycle_requested = true},
    sector,
    tuning
  );

  CHECK(hyperverse::has_locked_target(lock));
  CHECK(lock.target == near);
  CHECK(lock.wrapped_distance == Catch::Approx(75.0F));
  CHECK(lock.relative_position.x == Catch::Approx(75.0F));
  CHECK(lock.scan_confidence == Catch::Approx(0.62F));
}

TEST_CASE("target lock cancels and breaks after release range") {
  entt::registry registry;
  const entt::entity target = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(target, hyperverse::AsteroidBody{.position = {.x = 100.0F, .y = 100.0F}});

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 650.0F};

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {.cancel_requested = true}, sector, tuning);
  CHECK_FALSE(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 1200.0F, .y = 100.0F}, {}, sector, tuning);
  CHECK_FALSE(hyperverse::has_locked_target(lock));
}

TEST_CASE("grand central derives a minimal account context without exposing ownership") {
  std::ostringstream output;
  hyperverse::GrandCentral grand_central{output};
  hyperverse::AccountCtx account = grand_central.account_context();

  const entt::entity entity = account.registry().create();
  account.log().info(account.account().callsign());

  CHECK((entity != entt::null));
  CHECK(account.log().scope() == "account");
  CHECK(output.str().find("[account] Pioneer") != std::string::npos);
}
