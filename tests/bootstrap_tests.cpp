#include "hyperverse/version.hpp"

#include <boost/sml.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <entt/entity/registry.hpp>
#include <eventpp/eventqueue.h>

#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/cargo.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/grand_central.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/render_frame.hpp"
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
      .tool_intensity = 0.5F,
      .control_mapping = hyperverse::ControlMapping::Gamepad,
    });
  CHECK(hyperverse::length(moving.desired_movement) == Catch::Approx(1.0F));
  CHECK(moving.confirm_requested);
  CHECK(moving.target_cycle_requested);
  CHECK(moving.tool_intensity == Catch::Approx(0.5F));
  CHECK(moving.control_mapping == hyperverse::ControlMapping::Gamepad);
}

TEST_CASE("stateful flight input mapper emits button requests on rising edges") {
  hyperverse::FlightInputMapper mapper;

  const hyperverse::SemanticInputFrame pressed = mapper.map({.target_cycle = true});
  const hyperverse::SemanticInputFrame held = mapper.map({.target_cycle = true});
  const hyperverse::SemanticInputFrame released = mapper.map({});
  const hyperverse::SemanticInputFrame pressed_again = mapper.map({.target_cycle = true});

  CHECK(pressed.target_cycle_requested);
  CHECK_FALSE(held.target_cycle_requested);
  CHECK_FALSE(released.target_cycle_requested);
  CHECK(pressed_again.target_cycle_requested);
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

TEST_CASE("assisted flight turns the ship toward thrust direction") {
  hyperverse::ShipMotion ship{.facing_radians = 0.0F};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F, .acceleration = 100.0F, .braking = 200.0F, .turn_rate = 20.0F};

  hyperverse::simulate_assisted_flight(
    ship,
    {.desired_movement = {.x = 0.0F, .y = 1.0F}},
    flight,
    sector,
    1.0F
  );

  CHECK(ship.facing_radians == Catch::Approx(1.5707964F));
}

TEST_CASE("assisted flight prefers thrust facing over aim facing while moving") {
  hyperverse::ShipMotion ship{.facing_radians = 0.0F};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::FlightTuning flight{.max_speed = 100.0F, .acceleration = 100.0F, .braking = 200.0F, .turn_rate = 20.0F};

  hyperverse::simulate_assisted_flight(
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
    hyperverse::AsteroidBody{.position = {.x = 25.0F, .y = 4500.0F}, .radius = 25.0F, .scan_confidence = 0.62F}
  );

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 700.0F};

  hyperverse::update_target_lock(
    lock,
    registry,
    {.x = 8950.0F, .y = 4500.0F},
    {.x = 100.0F, .y = 0.0F},
    {.target_cycle_requested = true},
    sector,
    tuning
  );

  CHECK(hyperverse::has_locked_target(lock));
  CHECK(lock.target == near);
  CHECK(lock.wrapped_distance == Catch::Approx(75.0F));
  CHECK(lock.relative_position.x == Catch::Approx(75.0F));
  CHECK(lock.relative_velocity.x == Catch::Approx(-100.0F));
  CHECK(lock.closing_speed == Catch::Approx(100.0F));
  CHECK(lock.time_to_contact_seconds == Catch::Approx(0.5F));
  CHECK(lock.scan_confidence == Catch::Approx(0.62F));
}

TEST_CASE("target lock cancels and breaks after release range") {
  entt::registry registry;
  const entt::entity target = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(target, hyperverse::AsteroidBody{.position = {.x = 100.0F, .y = 100.0F}});

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 650.0F};

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {}, {.cancel_requested = true}, sector, tuning);
  CHECK_FALSE(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 120.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(hyperverse::has_locked_target(lock));

  hyperverse::update_target_lock(lock, registry, {.x = 1200.0F, .y = 100.0F}, {}, {}, sector, tuning);
  CHECK_FALSE(hyperverse::has_locked_target(lock));
}

TEST_CASE("target lock cycles to another asteroid in range") {
  entt::registry registry;
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(first, hyperverse::AsteroidBody{.position = {.x = 200.0F, .y = 100.0F}});
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(second, hyperverse::AsteroidBody{.position = {.x = 260.0F, .y = 100.0F}});

  hyperverse::TargetLockModel lock{};
  const hyperverse::SectorTuning sector{.width = 9000.0F, .height = 9000.0F};
  const hyperverse::TargetingTuning tuning{.lock_range = 500.0F, .release_range = 650.0F};

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(lock.target == first);

  hyperverse::update_target_lock(lock, registry, {.x = 100.0F, .y = 100.0F}, {}, {.target_cycle_requested = true}, sector, tuning);
  CHECK(lock.target == second);
  CHECK(lock.wrapped_distance == Catch::Approx(160.0F));
}

TEST_CASE("Vulkan clear color stays stable now that sprites expose state") {
  const hyperverse::RenderColor idle = hyperverse::make_clear_color({});
  const hyperverse::RenderColor fast = hyperverse::make_clear_color({.speed_fraction = 1.0F});
  const hyperverse::RenderColor locked = hyperverse::make_clear_color({.target_locked = true});
  const hyperverse::RenderColor mining = hyperverse::make_clear_color({.mining_active = true});
  const hyperverse::RenderColor warning = hyperverse::make_clear_color({.speed_fraction = 1.0F, .wrap_warning = true});

  CHECK(fast.r == Catch::Approx(idle.r));
  CHECK(locked.g == Catch::Approx(idle.g));
  CHECK(mining.b == Catch::Approx(idle.b));
  CHECK(warning.r == Catch::Approx(idle.r));
}

TEST_CASE("collision prediction warns before ship reaches asteroid") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 200.0F, .y = 100.0F}, .radius = 40.0F}
  );

  const hyperverse::ShipMotion ship{
    .position = {.x = 100.0F, .y = 100.0F},
    .velocity = {.x = 50.0F, .y = 0.0F},
  };
  const hyperverse::CollisionHudSnapshot collision = hyperverse::predict_ship_asteroid_collision(
    ship,
    registry,
    {.width = 9000.0F, .height = 9000.0F},
    {.ship_radius = 10.0F, .warning_seconds = 2.0F}
  );

  CHECK_FALSE(collision.contact);
  CHECK(collision.warning);
  CHECK(collision.asteroid == asteroid);
  CHECK(collision.separation == Catch::Approx(50.0F));
  CHECK(collision.impact_speed == Catch::Approx(50.0F));
  CHECK(collision.time_to_contact_seconds == Catch::Approx(1.0F));
}

TEST_CASE("collision prediction reports current contact") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    asteroid,
    hyperverse::AsteroidBody{.position = {.x = 110.0F, .y = 100.0F}, .radius = 40.0F}
  );

  const hyperverse::ShipMotion ship{
    .position = {.x = 100.0F, .y = 100.0F},
    .velocity = {.x = 5.0F, .y = 0.0F},
  };
  const hyperverse::CollisionHudSnapshot collision = hyperverse::predict_ship_asteroid_collision(
    ship,
    registry,
    {.width = 9000.0F, .height = 9000.0F},
    {.ship_radius = 10.0F}
  );

  CHECK(collision.contact);
  CHECK(collision.warning);
  CHECK(collision.separation == Catch::Approx(0.0F));
}

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

TEST_CASE("cargo manifest converts mined mass into quota status") {
  entt::registry registry;
  const entt::entity first = registry.create();
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::MiningResource>(first, hyperverse::MiningResource{.extracted_mass = 12.0F});
  registry.emplace<hyperverse::MiningResource>(second, hyperverse::MiningResource{.extracted_mass = 18.0F});
  hyperverse::CargoManifest manifest;

  const hyperverse::CargoHudSnapshot cargo = hyperverse::update_cargo_manifest(
    manifest,
    registry,
    {.required_mass = 40.0F, .cargo_box_mass = 10.0F, .over_quota_bonus_step_mass = 20.0F}
  );

  CHECK(manifest.delivered_mass == Catch::Approx(30.0F));
  CHECK(manifest.cargo_boxes == 3);
  CHECK(cargo.cargo_boxes == 3);
  CHECK(cargo.quota_fraction == Catch::Approx(0.75F));
  CHECK_FALSE(cargo.extraction_authorized);
}

TEST_CASE("cargo manifest authorizes extraction and calculates over-quota bonus") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::MiningResource>(asteroid, hyperverse::MiningResource{.extracted_mass = 72.0F});
  hyperverse::CargoManifest manifest;

  const hyperverse::CargoHudSnapshot cargo = hyperverse::update_cargo_manifest(
    manifest,
    registry,
    {.required_mass = 40.0F, .cargo_box_mass = 10.0F, .over_quota_bonus_step_mass = 16.0F, .bonus_per_step = 0.25F}
  );

  CHECK(cargo.extraction_authorized);
  CHECK(cargo.over_quota_mass == Catch::Approx(32.0F));
  CHECK(cargo.payout_multiplier == Catch::Approx(1.5F));
  CHECK(cargo.quota_fraction == Catch::Approx(1.0F));
}

TEST_CASE("cargo boxes are created at the extraction site from manifest mass") {
  entt::registry registry;
  const entt::entity asteroid = registry.create();
  registry.emplace<hyperverse::MiningResource>(asteroid, hyperverse::MiningResource{.extracted_mass = 35.0F});
  hyperverse::CargoManifest manifest;
  (void)hyperverse::update_cargo_manifest(manifest, registry, {.cargo_box_mass = 10.0F});

  const int box_count = hyperverse::sync_cargo_boxes(
    registry,
    manifest,
    {.position = {.x = 1000.0F, .y = 2000.0F}},
    {.box_mass = 10.0F, .box_spacing = 50.0F}
  );

  CHECK(box_count == 3);
  int counted_boxes = 0;
  bool saw_second_box = false;
  for (auto [entity, box] : registry.view<hyperverse::CargoBox>().each()) {
    (void)entity;
    ++counted_boxes;
    if (box.index == 1) {
      saw_second_box = true;
      CHECK(box.position.x == Catch::Approx(1050.0F));
      CHECK(box.position.y == Catch::Approx(2000.0F));
      CHECK(box.mass == Catch::Approx(10.0F));
    }
  }
  CHECK(counted_boxes == 3);
  CHECK(saw_second_box);
}

TEST_CASE("cargo escort arms when quota is authorized") {
  hyperverse::CargoEscortState escort;

  const hyperverse::CargoEscortHudSnapshot hud = hyperverse::update_cargo_escort_state(
    escort,
    {.extraction_authorized = true},
    {}
  );

  CHECK(escort.phase == hyperverse::CargoEscortPhase::Authorized);
  CHECK(hud.phase == hyperverse::CargoEscortPhase::Authorized);
  CHECK(hud.extraction_authorized);
  CHECK_FALSE(hud.cargo_train_active);
}

TEST_CASE("cargo escort activates on confirm after authorization") {
  hyperverse::CargoEscortState escort;

  const hyperverse::CargoEscortHudSnapshot active = hyperverse::update_cargo_escort_state(
    escort,
    {.extraction_authorized = true},
    {.confirm_requested = true}
  );

  CHECK(active.phase == hyperverse::CargoEscortPhase::EscortActive);
  CHECK(active.cargo_train_active);

  const hyperverse::CargoEscortHudSnapshot persistent = hyperverse::update_cargo_escort_state(escort, {}, {});

  CHECK(persistent.phase == hyperverse::CargoEscortPhase::EscortActive);
  CHECK(persistent.cargo_train_active);
}

TEST_CASE("inactive cargo train leaves extraction boxes parked") {
  entt::registry registry;
  const entt::entity box_entity = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    box_entity,
    hyperverse::CargoBox{.position = {.x = 1000.0F, .y = 1000.0F}, .index = 0}
  );

  const hyperverse::CargoTrainHudSnapshot hud = hyperverse::update_cargo_train(
    registry,
    {},
    {.position = {.x = 1200.0F, .y = 1000.0F}, .velocity = {.x = 100.0F, .y = 0.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    1.0F
  );

  const hyperverse::CargoBox& box = registry.get<hyperverse::CargoBox>(box_entity);
  CHECK_FALSE(hud.active);
  CHECK(hud.linked_boxes == 1);
  CHECK(box.position.x == Catch::Approx(1000.0F));
  CHECK(box.position.y == Catch::Approx(1000.0F));
}

TEST_CASE("active cargo train links boxes behind the ship in slot order") {
  entt::registry registry;
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    second,
    hyperverse::CargoBox{.position = {.x = 900.0F, .y = 1000.0F}, .index = 1}
  );
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    first,
    hyperverse::CargoBox{.position = {.x = 1000.0F, .y = 1000.0F}, .index = 0}
  );

  const hyperverse::CargoTrainHudSnapshot hud = hyperverse::update_cargo_train(
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 1000.0F, .y = 1000.0F}, .velocity = {.x = 100.0F, .y = 0.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    1.0F,
    {.link_spacing = 100.0F, .follow_rate = 1.0F, .max_speed = 1000.0F}
  );

  const hyperverse::CargoBox& first_box = registry.get<hyperverse::CargoBox>(first);
  const hyperverse::CargoBox& second_box = registry.get<hyperverse::CargoBox>(second);
  CHECK(hud.active);
  CHECK(hud.linked_boxes == 2);
  CHECK(hud.train_length == Catch::Approx(200.0F));
  CHECK(hud.max_coupling_stress == Catch::Approx(1.0F));
  CHECK(first_box.position.x == Catch::Approx(900.0F));
  CHECK(first_box.position.y == Catch::Approx(1000.0F));
  CHECK(second_box.position.x == Catch::Approx(800.0F));
  CHECK(second_box.position.y == Catch::Approx(1000.0F));
}

TEST_CASE("cargo escort route reports wrapped gate distance only while active") {
  const hyperverse::CargoEscortRouteHudSnapshot inactive = hyperverse::update_cargo_escort_route(
    {},
    {.gate_position = {.x = 25.0F, .y = 4500.0F}, .gate_radius = 80.0F},
    {.position = {.x = 8950.0F, .y = 4500.0F}},
    {.width = 9000.0F, .height = 9000.0F}
  );

  CHECK_FALSE(inactive.active);
  CHECK_FALSE(inactive.gate_reached);
  CHECK(inactive.gate_distance == Catch::Approx(75.0F));

  const hyperverse::CargoEscortRouteHudSnapshot active = hyperverse::update_cargo_escort_route(
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.gate_position = {.x = 25.0F, .y = 4500.0F}, .gate_radius = 80.0F},
    {.position = {.x = 8950.0F, .y = 4500.0F}},
    {.width = 9000.0F, .height = 9000.0F}
  );

  CHECK(active.active);
  CHECK(active.gate_reached);
  CHECK(active.gate_distance == Catch::Approx(75.0F));
}

TEST_CASE("cargo escort completes when the active train reaches the gate") {
  hyperverse::CargoEscortState escort{.phase = hyperverse::CargoEscortPhase::EscortActive};
  const hyperverse::CargoHudSnapshot cargo{.extraction_authorized = true};

  const hyperverse::CargoEscortHudSnapshot delivered = hyperverse::update_cargo_escort_arrival(
    escort,
    cargo,
    {.gate_distance = 20.0F, .active = true, .gate_reached = true}
  );

  CHECK(escort.phase == hyperverse::CargoEscortPhase::Complete);
  CHECK(delivered.phase == hyperverse::CargoEscortPhase::Complete);
  CHECK_FALSE(delivered.cargo_train_active);

  const hyperverse::CargoEscortHudSnapshot persistent = hyperverse::update_cargo_escort_state(escort, {}, {});

  CHECK(persistent.phase == hyperverse::CargoEscortPhase::Complete);
  CHECK_FALSE(persistent.cargo_train_active);
}

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

TEST_CASE("active cargo train ignores stolen boxes") {
  entt::registry registry;
  const entt::entity linked = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    linked,
    hyperverse::CargoBox{.position = {.x = 1000.0F, .y = 1000.0F}, .index = 0}
  );
  const entt::entity stolen = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    stolen,
    hyperverse::CargoBox{.position = {.x = 1200.0F, .y = 1000.0F}, .index = 1, .state = hyperverse::CargoBoxState::Stolen}
  );

  const hyperverse::CargoTrainHudSnapshot hud = hyperverse::update_cargo_train(
    registry,
    {.phase = hyperverse::CargoEscortPhase::EscortActive},
    {.position = {.x = 1000.0F, .y = 1000.0F}, .velocity = {.x = 100.0F, .y = 0.0F}},
    {.width = 9000.0F, .height = 9000.0F},
    1.0F,
    {.link_spacing = 100.0F, .follow_rate = 1.0F, .max_speed = 1000.0F}
  );

  const hyperverse::CargoBox& linked_box = registry.get<hyperverse::CargoBox>(linked);
  const hyperverse::CargoBox& stolen_box = registry.get<hyperverse::CargoBox>(stolen);
  CHECK(hud.linked_boxes == 1);
  CHECK(linked_box.position.x == Catch::Approx(900.0F));
  CHECK(stolen_box.position.x == Catch::Approx(1200.0F));
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

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {.phase = hyperverse::TargetLockPhase::Locked, .target = asteroid},
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    {.max_speed = 100.0F, .mining_range = 50.0F}
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
  hyperverse::MiningDrone drone{.position = {.x = 100.0F, .y = 100.0F}, .target = asteroid};

  const hyperverse::MiningDroneHudSnapshot hud = hyperverse::update_mining_drone(
    drone,
    registry,
    {},
    {.width = 9000.0F, .height = 9000.0F},
    2.0F,
    {.mining_range = 50.0F, .integrity_damage_per_second = 4.0F, .extraction_per_second = 3.0F}
  );

  const hyperverse::MiningResource& resource = registry.get<hyperverse::MiningResource>(asteroid);
  CHECK(drone.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(hud.phase == hyperverse::MiningDronePhase::Mining);
  CHECK(resource.integrity == Catch::Approx(92.0F));
  CHECK(resource.extracted_mass == Catch::Approx(6.0F));
  CHECK(drone.extracted_mass == Catch::Approx(6.0F));
}

TEST_CASE("sector pressure escalates on a tunable interval") {
  hyperverse::SectorPressureModel pressure;
  const hyperverse::SectorPressureTuning tuning{
    .escalation_interval_seconds = 10.0F,
    .announcement_duration_seconds = 3.0F,
    .pressure_per_level = 0.25F,
  };

  const hyperverse::SectorPressureHudSnapshot before = hyperverse::update_sector_pressure(pressure, 9.0F, tuning);
  CHECK(before.escalation_level == 0);
  CHECK(before.next_escalation_seconds == Catch::Approx(1.0F));
  CHECK_FALSE(before.escalation_announced);

  const hyperverse::SectorPressureHudSnapshot after = hyperverse::update_sector_pressure(pressure, 1.0F, tuning);
  CHECK(after.escalation_level == 1);
  CHECK(after.pressure_fraction == Catch::Approx(0.25F));
  CHECK(after.escalation_announced);
}

TEST_CASE("sector pressure announcement expires after the HUD window") {
  hyperverse::SectorPressureModel pressure;
  const hyperverse::SectorPressureTuning tuning{
    .escalation_interval_seconds = 10.0F,
    .announcement_duration_seconds = 3.0F,
  };

  (void)hyperverse::update_sector_pressure(pressure, 10.0F, tuning);
  const hyperverse::SectorPressureHudSnapshot expired = hyperverse::update_sector_pressure(pressure, 3.1F, tuning);

  CHECK(expired.escalation_level == 1);
  CHECK_FALSE(expired.escalation_announced);
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
