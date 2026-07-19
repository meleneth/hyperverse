#include "test_common.hpp"

#include "hyperverse/radar_control.hpp"

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
      .enemy_target_cycle = true,
      .boost = true,
      .gravity_sling = true,
      .particle_fire = true,
      .missile_fire = true,
      .tool_intensity = 0.5F,
      .control_mapping = hyperverse::ControlMapping::Gamepad,
    });
  CHECK(hyperverse::length(moving.desired_movement) == Catch::Approx(1.0F));
  CHECK(moving.confirm_requested);
  CHECK(moving.target_cycle_active);
  CHECK(moving.target_cycle_requested);
  CHECK(moving.enemy_target_cycle_active);
  CHECK(moving.enemy_target_cycle_requested);
  CHECK(moving.clear_targets_active);
  CHECK(moving.clear_targets_requested);
  CHECK(moving.boost_requested);
  CHECK(moving.gravity_sling_requested);
  CHECK(moving.particle_fire_requested);
  CHECK(moving.particle_fire_active);
  CHECK(moving.missile_fire_requested);
  CHECK(moving.missile_fire_active);
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

  const hyperverse::SemanticInputFrame missile = mapper.map({.missile_fire = true});
  const hyperverse::SemanticInputFrame held_missile = mapper.map({.missile_fire = true});
  CHECK(missile.missile_fire_requested);
  CHECK(missile.missile_fire_active);
  CHECK_FALSE(held_missile.missile_fire_requested);
  CHECK(held_missile.missile_fire_active);

  const hyperverse::SemanticInputFrame boosted = mapper.map({.boost = true});
  const hyperverse::SemanticInputFrame held_boost = mapper.map({.boost = true});
  CHECK(boosted.boost_requested);
  CHECK_FALSE(held_boost.boost_requested);

  const hyperverse::SemanticInputFrame slung = mapper.map({.gravity_sling = true});
  const hyperverse::SemanticInputFrame held_sling = mapper.map({.gravity_sling = true});
  CHECK(slung.gravity_sling_requested);
  CHECK_FALSE(held_sling.gravity_sling_requested);
}

TEST_CASE("stateful flight input mapper emits enemy target cycle on left shoulder edge") {
  hyperverse::FlightInputMapper mapper;

  const hyperverse::SemanticInputFrame pressed = mapper.map({.enemy_target_cycle = true});
  const hyperverse::SemanticInputFrame held = mapper.map({.enemy_target_cycle = true});
  const hyperverse::SemanticInputFrame released = mapper.map({});
  const hyperverse::SemanticInputFrame pressed_again = mapper.map({.enemy_target_cycle = true});

  CHECK(pressed.enemy_target_cycle_active);
  CHECK(pressed.enemy_target_cycle_requested);
  CHECK(held.enemy_target_cycle_active);
  CHECK_FALSE(held.enemy_target_cycle_requested);
  CHECK_FALSE(released.enemy_target_cycle_requested);
  CHECK(pressed_again.enemy_target_cycle_requested);
}

TEST_CASE("radar control FSM emits mining and enemy target cycle events") {
  hyperverse::RadarControlModel control;
  hyperverse::DomainEventBus events;
  int mining_events = 0;
  int enemy_events = 0;
  events.appendListener(hyperverse::DomainEventType::MiningTargetCycleRequested, [&](const hyperverse::DomainEvent&) { ++mining_events; });
  events.appendListener(hyperverse::DomainEventType::EnemyTargetCycleRequested, [&](const hyperverse::DomainEvent&) { ++enemy_events; });

  hyperverse::RadarControlFrame mining = hyperverse::update_radar_control(
    control,
    {.target_cycle_active = true},
    &events
  );
  events.process();
  CHECK(control.phase == hyperverse::RadarControlPhase::SingleHeld);
  CHECK(control.focus == hyperverse::RadarFocus::Mining);
  CHECK(mining.mining_target_cycle_requested);
  CHECK_FALSE(mining.enemy_target_cycle_requested);
  CHECK(mining_events == 1);

  hyperverse::RadarControlFrame held = hyperverse::update_radar_control(
    control,
    {.target_cycle_active = true},
    &events
  );
  events.process();
  CHECK_FALSE(held.mining_target_cycle_requested);
  CHECK(mining_events == 1);

  (void)hyperverse::update_radar_control(control, {}, &events);
  hyperverse::RadarControlFrame enemy = hyperverse::update_radar_control(
    control,
    {.enemy_target_cycle_active = true},
    &events
  );
  events.process();
  CHECK(enemy.enemy_target_cycle_requested);
  CHECK(control.focus == hyperverse::RadarFocus::Combat);
  CHECK(enemy_events == 1);
}

TEST_CASE("radar control FSM blocks sloppy shoulder release after clear chord") {
  hyperverse::RadarControlModel control;
  hyperverse::DomainEventBus events;
  int clear_events = 0;
  events.appendListener(hyperverse::DomainEventType::RadarTargetsCleared, [&](const hyperverse::DomainEvent&) { ++clear_events; });

  hyperverse::RadarControlFrame clear = hyperverse::update_radar_control(
    control,
    {.target_cycle_active = true, .enemy_target_cycle_active = true, .clear_targets_active = true},
    &events
  );
  events.process();
  CHECK(control.phase == hyperverse::RadarControlPhase::ChordBlocked);
  CHECK(control.focus == hyperverse::RadarFocus::None);
  CHECK(clear.clear_targets_requested);
  CHECK(clear_events == 1);

  hyperverse::RadarControlFrame sloppy_right = hyperverse::update_radar_control(
    control,
    {.target_cycle_active = true},
    &events
  );
  events.process();
  CHECK(control.phase == hyperverse::RadarControlPhase::ChordBlocked);
  CHECK_FALSE(sloppy_right.mining_target_cycle_requested);
  CHECK_FALSE(sloppy_right.enemy_target_cycle_requested);
  CHECK(clear_events == 1);

  hyperverse::RadarControlFrame sloppy_left = hyperverse::update_radar_control(
    control,
    {.enemy_target_cycle_active = true},
    &events
  );
  events.process();
  CHECK(control.phase == hyperverse::RadarControlPhase::ChordBlocked);
  CHECK_FALSE(sloppy_left.mining_target_cycle_requested);
  CHECK_FALSE(sloppy_left.enemy_target_cycle_requested);

  (void)hyperverse::update_radar_control(control, {}, &events);
  CHECK(control.phase == hyperverse::RadarControlPhase::Released);

  hyperverse::RadarControlFrame clean_right = hyperverse::update_radar_control(
    control,
    {.target_cycle_active = true},
    &events
  );
  CHECK(clean_right.mining_target_cycle_requested);
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
