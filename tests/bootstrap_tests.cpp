#include "test_common.hpp"

#include "hyperverse/vertical_slice_seed.hpp"

using hyperverse::test::TestAccountWorld;

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

TEST_CASE("vertical slice asteroids start six times their depleted size") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  (void)hyperverse::seed_vertical_slice(account);

  bool found_asteroid = false;
  for (auto [entity, asteroid] : account.registry().view<hyperverse::AsteroidBody>().each()) {
    (void)entity;
    found_asteroid = true;
    CHECK(asteroid.radius == Catch::Approx(asteroid.base_radius));
    CHECK(asteroid.base_radius / 6.0F >= 75.0F);
  }

  CHECK(found_asteroid);
}

TEST_CASE("vertical slice asteroid field is spread across the sector and visibly moving") {
  TestAccountWorld world;
  hyperverse::AccountCtx account = world.account_context();
  (void)hyperverse::seed_vertical_slice(account);

  int asteroid_count = 0;
  int initially_visible_count = 0;
  constexpr float left = 2670.0F;
  constexpr float right = 6330.0F;
  constexpr float top = 2950.0F;
  constexpr float bottom = 5020.0F;

  for (auto [entity, asteroid] : account.registry().view<hyperverse::AsteroidBody>().each()) {
    (void)entity;
    ++asteroid_count;
    CHECK(hyperverse::length(asteroid.velocity) >= 180.0F);
    if (asteroid.position.x >= left && asteroid.position.x <= right && asteroid.position.y >= top && asteroid.position.y <= bottom) {
      ++initially_visible_count;
    }
  }

  CHECK(asteroid_count >= 24);
  CHECK(initially_visible_count <= 4);
}
