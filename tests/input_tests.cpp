#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

TEST_CASE("flight input maps raw devices into semantic movement intent") {
  const hyperverse::SemanticInputFrame idle = hyperverse::map_flight_intent({.movement_axis = {.x = 0.05F, .y = 0.0F}});
  CHECK(idle.desired_movement.x == Catch::Approx(0.0F));
  CHECK(idle.desired_movement.y == Catch::Approx(0.0F));

  const hyperverse::SemanticInputFrame moving =
    hyperverse::map_flight_intent({
      .movement_axis = {.x = 1.0F, .y = 1.0F},
      .confirm = true,
      .target_cycle = true,
      .boost = true,
      .particle_fire = true,
      .tool_intensity = 0.5F,
      .control_mapping = hyperverse::ControlMapping::Gamepad,
    });
  CHECK(hyperverse::length(moving.desired_movement) == Catch::Approx(1.0F));
  CHECK(moving.confirm_requested);
  CHECK(moving.target_cycle_requested);
  CHECK(moving.boost_requested);
  CHECK(moving.particle_fire_requested);
  CHECK(moving.particle_fire_active);
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

  const hyperverse::SemanticInputFrame fired = mapper.map({.particle_fire = true});
  const hyperverse::SemanticInputFrame held_fire = mapper.map({.particle_fire = true});
  CHECK(fired.particle_fire_requested);
  CHECK(fired.particle_fire_active);
  CHECK_FALSE(held_fire.particle_fire_requested);
  CHECK(held_fire.particle_fire_active);

  const hyperverse::SemanticInputFrame boosted = mapper.map({.boost = true});
  const hyperverse::SemanticInputFrame held_boost = mapper.map({.boost = true});
  CHECK(boosted.boost_requested);
  CHECK_FALSE(held_boost.boost_requested);
}

TEST_CASE("flight input mapper uses a state machine for active device mapping") {
  hyperverse::FlightInputMapper mapper;

  CHECK(mapper.active_mapping() == hyperverse::ControlMapping::Keyboard);
  CHECK(mapper.map({.control_mapping = hyperverse::ControlMapping::Gamepad}).control_mapping == hyperverse::ControlMapping::Gamepad);
  CHECK(mapper.active_mapping() == hyperverse::ControlMapping::Gamepad);
  CHECK(mapper.map({.control_mapping = hyperverse::ControlMapping::Keyboard}).control_mapping == hyperverse::ControlMapping::Keyboard);
}

TEST_CASE("mineral composition is an explicit asteroid component input") {
  const hyperverse::MineralComposition exotic = hyperverse::mineral_composition_for_tier(hyperverse::OreTier::Exotic);
  const hyperverse::MineralComposition anomalous = hyperverse::mineral_composition_for_tier(hyperverse::OreTier::Anomalous);

  CHECK(exotic.exotic_crystal > exotic.silicate);
  CHECK(anomalous.anomalous_matter > 0.0F);
  CHECK(hyperverse::ore_tint(anomalous).g > hyperverse::ore_tint(exotic).g);
}
