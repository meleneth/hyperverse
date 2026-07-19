#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("assisted flight accelerates, brakes, and wraps through the sector") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  hyperverse::ShipMotion ship{.position = {.x = 8995.0F, .y = 4500.0F}};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F, .acceleration = 100.0F, .braking = 200.0F, .turn_rate = 20.0F};

  hyperverse::simulate_assisted_flight(
    account,
    ship,
    {.desired_movement = {.x = 1.0F, .y = 0.0F}},
    flight,
    sector,
    1.0F
  );

  CHECK(ship.velocity.x == Catch::Approx(100.0F));
  CHECK(ship.position.x == Catch::Approx(95.0F));

  hyperverse::simulate_assisted_flight(account, ship, {}, flight, sector, 1.0F);
  CHECK(hyperverse::length(ship.velocity) == Catch::Approx(0.0F));
}

TEST_CASE("strict thruster physics does not accelerate without thrust") {
  hyperverse::ShipMotion ship{.position = {.x = 100.0F, .y = 100.0F}, .velocity = {.x = 40.0F, .y = 0.0F}};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};

  hyperverse::apply_thruster_physics(ship, {}, {.max_speed = 1000.0F}, sector, 1.0F);

  CHECK(ship.velocity.x == Catch::Approx(40.0F));
  CHECK(ship.position.x == Catch::Approx(140.0F));
}

TEST_CASE("assisted flight turns the ship toward thrust direction") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  hyperverse::ShipMotion ship{.facing_radians = 0.0F};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F, .acceleration = 100.0F, .braking = 200.0F, .turn_rate = 20.0F};

  hyperverse::simulate_assisted_flight(
    account,
    ship,
    {.desired_movement = {.x = 0.0F, .y = 1.0F}},
    flight,
    sector,
    1.0F
  );

  CHECK(ship.facing_radians == Catch::Approx(1.5707964F));
}

TEST_CASE("boost increases thrust authority instead of setting speed directly") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  hyperverse::ShipMotion ship{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{
    .max_speed = 100.0F,
    .acceleration = 100.0F,
    .braking = 0.0F,
    .turn_rate = 20.0F,
    .boost_speed_multiplier = 2.0F,
    .boost_duration_seconds = 0.33F,
  };

  hyperverse::simulate_assisted_flight(
    account,
    ship,
    {.desired_movement = {.x = 1.0F, .y = 0.0F}, .boost_requested = true},
    flight,
    sector,
    0.165F
  );

  CHECK(hyperverse::length(ship.velocity) == Catch::Approx(33.0F));
  CHECK(ship.boost_speed == Catch::Approx(87.5F));

  hyperverse::simulate_assisted_flight(account, ship, {}, flight, sector, 0.165F);

  CHECK(ship.boost_speed == Catch::Approx(0.0F));
  CHECK(hyperverse::length(ship.velocity) == Catch::Approx(33.0F));
}

TEST_CASE("assisted flight prefers thrust facing over aim facing while moving") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  hyperverse::ShipMotion ship{.facing_radians = 0.0F};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F, .acceleration = 100.0F, .braking = 200.0F, .turn_rate = 20.0F};

  hyperverse::simulate_assisted_flight(
    account,
    ship,
    {
      .desired_movement = {.x = 1.0F, .y = 0.0F},
      .primary_aim = {.x = 0.0F, .y = 1.0F},
    },
    flight,
    sector,
    1.0F
  );

  CHECK(ship.facing_radians == Catch::Approx(0.0F));
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
  CHECK(snapshot.thrust_vector.x == Catch::Approx(1.0F));
  CHECK_FALSE(snapshot.braking_assist);

  const hyperverse::FlightHudSnapshot braking_snapshot = hyperverse::make_flight_hud_snapshot(ship, {}, flight, sector, hud);
  CHECK(braking_snapshot.thrust_vector.x == Catch::Approx(-1.0F));
  CHECK(braking_snapshot.braking_assist);
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
