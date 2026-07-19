#include "test_common.hpp"

#include <cmath>

namespace {

constexpr hyperverse::SectorTuning Sector{.width = 9000.0F, .height = 9000.0F};

entt::entity asteroid(
  entt::registry& registry,
  hyperverse::Vec2 position,
  hyperverse::Vec2 velocity = {},
  float radius = 100.0F,
  float rotation = 0.0F,
  float angular_velocity = 0.0F
) {
  const entt::entity entity = registry.create();
  registry.emplace<hyperverse::AsteroidBody>(
    entity,
    hyperverse::AsteroidBody{
      .position = position,
      .velocity = velocity,
      .radius = radius,
      .rotation_radians = rotation,
      .angular_velocity = angular_velocity,
    }
  );
  return entity;
}

void activate(
  hyperverse::GravitySlingModel& model,
  entt::registry& registry,
  hyperverse::ShipMotion& ship,
  const hyperverse::SemanticInputFrame& input = {}
) {
  (void)hyperverse::update_gravity_sling(
    model,
    registry,
    ship,
    {.primary_aim = {.x = 1.0F, .y = 0.0F}, .gravity_sling_requested = true},
    Sector,
    0.0F,
    {.engagement_seconds = 0.1F}
  );
  (void)hyperverse::update_gravity_sling(model, registry, ship, input, Sector, 0.1F, {.engagement_seconds = 0.1F});
}

}  // namespace

TEST_CASE("gravity sling acquisition deterministically prefers aim direction") {
  entt::registry registry;
  const entt::entity aimed = asteroid(registry, {.x = 800.0F, .y = 0.0F});
  const entt::entity nearer_off_axis = asteroid(registry, {.x = 100.0F, .y = 500.0F});

  const entt::entity acquired = hyperverse::acquire_gravity_sling_target(
    registry,
    {},
    {.x = 1.0F, .y = 0.0F},
    Sector,
    {.acquisition_range = 1600.0F}
  );

  CHECK(acquired == aimed);
  CHECK(acquired != nearer_off_axis);
}

TEST_CASE("gravity sling acquisition reports no target available") {
  entt::registry registry;
  asteroid(registry, {.x = 3000.0F, .y = 0.0F});

  CHECK((hyperverse::acquire_gravity_sling_target(registry, {}, {.x = 1.0F, .y = 0.0F}, Sector, {.acquisition_range = 1000.0F}) == entt::null));

  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{};
  const hyperverse::GravitySlingHudSnapshot hud = hyperverse::update_gravity_sling(
    model,
    registry,
    ship,
    {.primary_aim = {.x = 1.0F, .y = 0.0F}, .gravity_sling_requested = true},
    Sector,
    0.0F,
    {.acquisition_range = 1000.0F}
  );

  CHECK(model.phase == hyperverse::GravitySlingPhase::FreeFlight);
  CHECK(hud.acquisition_failed);
}

TEST_CASE("gravity sling transitions from free flight to engagement") {
  entt::registry registry;
  const entt::entity target = asteroid(registry, {.x = 500.0F, .y = 0.0F}, {}, 100.0F, 0.25F, 2.0F);
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}, .velocity = {.x = 10.0F, .y = 20.0F}};

  const hyperverse::GravitySlingHudSnapshot hud = hyperverse::update_gravity_sling(
    model,
    registry,
    ship,
    {.primary_aim = {.x = 1.0F, .y = 0.0F}, .gravity_sling_requested = true},
    Sector,
    0.0F
  );

  CHECK(model.phase == hyperverse::GravitySlingPhase::Engaging);
  CHECK(model.target == target);
  CHECK(model.entry_velocity.x == Catch::Approx(10.0F));
  CHECK(hud.phase == hyperverse::GravitySlingPhase::Engaging);
}

TEST_CASE("gravity sling transitions from engagement to active") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};

  activate(model, registry, ship);

  CHECK(model.phase == hyperverse::GravitySlingPhase::Active);
}

TEST_CASE("gravity sling locks the exact acquired asteroid distance") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F}, {}, 100.0F);
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 280.0F, .y = 0.0F}};

  (void)hyperverse::update_gravity_sling(
    model,
    registry,
    ship,
    {.primary_aim = {.x = 1.0F}, .gravity_sling_requested = true},
    Sector,
    0.0F,
    {.min_radius_padding = 260.0F, .max_radius_padding = 920.0F}
  );

  CHECK(model.radius == Catch::Approx(220.0F));
}

TEST_CASE("gravity sling phase FSM emits transition events") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};
  hyperverse::DomainEventBus event_bus;
  std::vector<hyperverse::DomainEvent> events;
  event_bus.appendListener(
    hyperverse::DomainEventType::GravitySlingPhaseChanged,
    [&](const hyperverse::DomainEvent& event) { events.push_back(event); }
  );

  (void)hyperverse::update_gravity_sling(
    model,
    registry,
    ship,
    {.primary_aim = {.x = 1.0F}, .gravity_sling_requested = true},
    Sector,
    0.0F,
    {.engagement_seconds = 0.1F},
    entt::entity{11},
    &event_bus
  );
  (void)hyperverse::update_gravity_sling(model, registry, ship, {}, Sector, 0.1F, {.engagement_seconds = 0.1F}, entt::entity{11}, &event_bus);
  (void)hyperverse::update_gravity_sling(model, registry, ship, {.gravity_sling_requested = true}, Sector, 0.1F, {}, entt::entity{11}, &event_bus);
  event_bus.process();

  REQUIRE(events.size() == 3);
  CHECK(events[0].subject == entt::entity{11});
  CHECK(events[0].count == static_cast<int>(hyperverse::GravitySlingPhase::Engaging));
  CHECK(events[1].count == static_cast<int>(hyperverse::GravitySlingPhase::Active));
  CHECK(events[2].count == static_cast<int>(hyperverse::GravitySlingPhase::FreeFlight));
  CHECK(events[2].amount == Catch::Approx(static_cast<float>(hyperverse::GravitySlingDisengageReason::PlayerReleased)));
}

TEST_CASE("gravity sling preserves stable relative position under target translation") {
  entt::registry registry;
  const entt::entity target = asteroid(registry, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};
  activate(model, registry, ship);
  const hyperverse::Vec2 before = hyperverse::wrapped_delta(registry.get<hyperverse::AsteroidBody>(target).position, ship.position, Sector);

  registry.get<hyperverse::AsteroidBody>(target).position.x += 120.0F;
  (void)hyperverse::update_gravity_sling(model, registry, ship, {}, Sector, 0.1F);
  const hyperverse::Vec2 after = hyperverse::wrapped_delta(registry.get<hyperverse::AsteroidBody>(target).position, ship.position, Sector);

  CHECK(after.x == Catch::Approx(before.x).margin(0.01F));
  CHECK(after.y == Catch::Approx(before.y).margin(0.01F));
}

TEST_CASE("gravity sling ignores target tumble when preserving bearing") {
  entt::registry registry;
  const entt::entity target = asteroid(registry, {.x = 500.0F, .y = 0.0F}, {}, 100.0F, 0.0F);
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};
  activate(model, registry, ship);
  const float local_angle = model.local_angle_radians;
  const hyperverse::Vec2 before = hyperverse::wrapped_delta(registry.get<hyperverse::AsteroidBody>(target).position, ship.position, Sector);

  registry.get<hyperverse::AsteroidBody>(target).rotation_radians += 1.0F;
  (void)hyperverse::update_gravity_sling(model, registry, ship, {}, Sector, 0.1F);
  const hyperverse::Vec2 after = hyperverse::wrapped_delta(registry.get<hyperverse::AsteroidBody>(target).position, ship.position, Sector);

  CHECK(model.local_angle_radians == Catch::Approx(local_angle).margin(0.001F));
  CHECK(after.x == Catch::Approx(before.x).margin(0.01F));
  CHECK(after.y == Catch::Approx(before.y).margin(0.01F));
}

TEST_CASE("gravity sling radial input cannot change constrained radius") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F}, {}, 100.0F);
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};
  const hyperverse::GravitySlingTuning tuning{.engagement_seconds = 0.1F, .min_radius_padding = 50.0F, .max_radius_padding = 200.0F, .radial_adjust_speed = 1000.0F};
  (void)hyperverse::update_gravity_sling(model, registry, ship, {.primary_aim = {.x = 1.0F}, .gravity_sling_requested = true}, Sector, 0.0F, tuning);
  (void)hyperverse::update_gravity_sling(model, registry, ship, {}, Sector, 0.1F, tuning);

  (void)hyperverse::update_gravity_sling(model, registry, ship, {.desired_movement = {.x = -1.0F}}, Sector, 1.0F, tuning);
  CHECK(model.radius == Catch::Approx(250.0F));
  (void)hyperverse::update_gravity_sling(model, registry, ship, {.desired_movement = {.x = 1.0F}}, Sector, 1.0F, tuning);
  CHECK(model.radius == Catch::Approx(250.0F));
}

TEST_CASE("gravity sling keeps radius locked while thrusting against asteroid travel") {
  entt::registry registry;
  const entt::entity target = asteroid(registry, {.x = 500.0F, .y = 0.0F}, {.x = 120.0F, .y = 0.0F}, 100.0F);
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}, .facing_radians = 0.0F};
  activate(model, registry, ship);
  const float locked_radius = model.radius;
  const float speed_before = registry.get<hyperverse::AsteroidBody>(target).velocity.x;

  (void)hyperverse::update_gravity_sling(
    model,
    registry,
    ship,
    {.desired_movement = {.x = -1.0F, .y = 0.0F}},
    Sector,
    0.25F,
    {.thrust_turn_rate = 100.0F, .asteroid_thrust_acceleration = 160.0F}
  );

  CHECK(model.radius == Catch::Approx(locked_radius));
  CHECK(hyperverse::wrapped_distance(registry.get<hyperverse::AsteroidBody>(target).position, ship.position, Sector) == Catch::Approx(locked_radius).margin(0.01F));
  CHECK(registry.get<hyperverse::AsteroidBody>(target).velocity.x < speed_before);
  CHECK(std::abs(std::abs(ship.facing_radians) - 3.14159265F) < 0.01F);
}

TEST_CASE("gravity sling angular adjustment changes local bearing") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};
  activate(model, registry, ship);
  const float before = model.local_angle_radians;

  (void)hyperverse::update_gravity_sling(model, registry, ship, {.desired_movement = {.y = -1.0F}}, Sector, 0.5F, {.angular_adjust_speed = 1.0F, .relative_angular_damping = 100.0F});

  CHECK(model.local_angle_radians != Catch::Approx(before));
}

TEST_CASE("gravity sling counter thrust slows relative orbit") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}, .velocity = {.x = 0.0F, .y = -200.0F}};
  activate(model, registry, ship);
  REQUIRE(model.relative_angular_velocity > 0.0F);
  const float before = model.relative_angular_velocity;

  (void)hyperverse::update_gravity_sling(model, registry, ship, {.desired_movement = {.y = 1.0F}}, Sector, 0.5F, {.angular_adjust_speed = 1.0F});

  CHECK(model.relative_angular_velocity < before);
}

TEST_CASE("gravity sling release velocity from stationary target") {
  const hyperverse::Vec2 velocity = hyperverse::gravity_sling_release_velocity({}, 0.0F, 200.0F, 1.5F);
  CHECK(velocity.x == Catch::Approx(0.0F));
  CHECK(velocity.y == Catch::Approx(300.0F));
}

TEST_CASE("gravity sling release velocity from translating target") {
  const hyperverse::Vec2 velocity =
    hyperverse::gravity_sling_release_velocity({.velocity = {.x = 40.0F, .y = -10.0F}}, 0.0F, 200.0F, 0.0F);
  CHECK(velocity.x == Catch::Approx(40.0F));
  CHECK(velocity.y == Catch::Approx(-10.0F));
}

TEST_CASE("gravity sling release velocity from rotating target") {
  const hyperverse::Vec2 velocity =
    hyperverse::gravity_sling_release_velocity({.angular_velocity = 2.0F}, 0.0F, 150.0F, 0.0F);
  CHECK(velocity.y == Catch::Approx(0.0F));
}

TEST_CASE("gravity sling release velocity combines translation and relative orbit") {
  const hyperverse::Vec2 velocity =
    hyperverse::gravity_sling_release_velocity({.velocity = {.x = 40.0F, .y = 5.0F}, .angular_velocity = 2.0F}, 0.0F, 150.0F, 1.0F);
  CHECK(velocity.x == Catch::Approx(40.0F));
  CHECK(velocity.y == Catch::Approx(155.0F));
}

TEST_CASE("gravity sling target destruction while slinging preserves instantaneous velocity") {
  entt::registry registry;
  const entt::entity target = asteroid(registry, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}};
  activate(model, registry, ship);
  model.current_world_velocity = {.x = 12.0F, .y = 34.0F};

  registry.destroy(target);
  (void)hyperverse::update_gravity_sling(model, registry, ship, {}, Sector, 0.1F);

  CHECK(model.phase == hyperverse::GravitySlingPhase::FreeFlight);
  CHECK(model.disengage_reason == hyperverse::GravitySlingDisengageReason::TargetDestroyed);
  CHECK(ship.velocity.x == Catch::Approx(12.0F));
  CHECK(ship.velocity.y == Catch::Approx(34.0F));
}

TEST_CASE("gravity sling explicit player release returns to free flight") {
  entt::registry registry;
  asteroid(registry, {.x = 500.0F, .y = 0.0F}, {}, 100.0F, 0.0F, 2.0F);
  hyperverse::GravitySlingModel model{};
  hyperverse::ShipMotion ship{.position = {.x = 250.0F, .y = 0.0F}, .velocity = {.x = 0.0F, .y = -80.0F}};
  activate(model, registry, ship);

  (void)hyperverse::update_gravity_sling(model, registry, ship, {.gravity_sling_requested = true}, Sector, 0.1F);

  CHECK(model.phase == hyperverse::GravitySlingPhase::FreeFlight);
  CHECK(model.disengage_reason == hyperverse::GravitySlingDisengageReason::PlayerReleased);
  CHECK(hyperverse::length(ship.velocity) > 0.0F);
}

TEST_CASE("gravity sling invalid stale target cleans up safely") {
  entt::registry registry;
  hyperverse::GravitySlingModel model{.phase = hyperverse::GravitySlingPhase::Active, .target = entt::entity{42}, .current_world_velocity = {.x = 3.0F}};
  hyperverse::ShipMotion ship{};

  (void)hyperverse::update_gravity_sling(model, registry, ship, {}, Sector, 0.1F);

  CHECK(model.phase == hyperverse::GravitySlingPhase::FreeFlight);
  CHECK(model.disengage_reason == hyperverse::GravitySlingDisengageReason::TargetDestroyed);
  CHECK(ship.velocity.x == Catch::Approx(3.0F));
}

TEST_CASE("gravity sling motion is frame-rate independent across representative time steps") {
  entt::registry registry_a;
  const entt::entity target_a = asteroid(registry_a, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model_a{};
  hyperverse::ShipMotion ship_a{.position = {.x = 250.0F, .y = 0.0F}};
  activate(model_a, registry_a, ship_a);

  entt::registry registry_b;
  const entt::entity target_b = asteroid(registry_b, {.x = 500.0F, .y = 0.0F});
  hyperverse::GravitySlingModel model_b{};
  hyperverse::ShipMotion ship_b{.position = {.x = 250.0F, .y = 0.0F}};
  activate(model_b, registry_b, ship_b);

  registry_a.get<hyperverse::AsteroidBody>(target_a).rotation_radians += 1.0F;
  (void)hyperverse::update_gravity_sling(model_a, registry_a, ship_a, {}, Sector, 1.0F);
  for (int step = 0; step < 10; ++step) {
    registry_b.get<hyperverse::AsteroidBody>(target_b).rotation_radians += 0.1F;
    (void)hyperverse::update_gravity_sling(model_b, registry_b, ship_b, {}, Sector, 0.1F);
  }

  CHECK(registry_a.valid(target_a));
  CHECK(registry_b.valid(target_b));
  CHECK(model_a.local_angle_radians == Catch::Approx(model_b.local_angle_radians).margin(0.001F));
  CHECK(ship_a.position.x == Catch::Approx(ship_b.position.x).margin(0.01F));
  CHECK(ship_a.position.y == Catch::Approx(ship_b.position.y).margin(0.01F));
}
