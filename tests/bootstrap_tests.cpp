#include "test_common.hpp"

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
