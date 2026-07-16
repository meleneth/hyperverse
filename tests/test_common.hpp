#pragma once

#include "hyperverse/version.hpp"

#include <boost/sml.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <entt/entity/registry.hpp>
#include <eventpp/eventqueue.h>

#include "hyperverse/account_context.hpp"
#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/cargo.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/projectile.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/render_frame.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/ship_status.hpp"
#include "hyperverse/targeting.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace hyperverse::test {

class TestAccountWorld {
public:
  TestAccountWorld() : log{output, "account"}, account{"Pioneer"} {}

  [[nodiscard]] AccountCtx account_context() {
    return {registry, rng, log, account, physics, event_bus};
  }

  std::ostringstream output;
  entt::registry registry;
  std::mt19937 rng{0x48595045U};
  ScopedLog log;
  AccountState account;
  PhysicsWorld physics;
  DomainEventBus event_bus;
};

}  // namespace hyperverse::test
