#include "test_common.hpp"

using hyperverse::test::TestAccountWorld;

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

TEST_CASE("cargo manifest calculates cash and score by ore tier") {
  entt::registry registry;
  const entt::entity common = registry.create();
  const entt::entity rare = registry.create();
  registry.emplace<hyperverse::MiningResource>(common, hyperverse::MiningResource{.tier = hyperverse::OreTier::Common, .extracted_mass = 10.0F});
  registry.emplace<hyperverse::MiningResource>(rare, hyperverse::MiningResource{.tier = hyperverse::OreTier::Rare, .extracted_mass = 4.0F});
  hyperverse::CargoManifest manifest;

  const hyperverse::CargoHudSnapshot cargo = hyperverse::update_cargo_manifest(
    manifest,
    registry,
    {
      .cash_per_mass = {1.0F, 2.0F, 5.0F, 12.0F, 30.0F},
      .score_per_cash = 10.0F,
    }
  );

  CHECK(cargo.cash == Catch::Approx(30.0F));
  CHECK(cargo.score == 300);
  CHECK(manifest.delivered_mass_by_tier[static_cast<std::size_t>(hyperverse::OreTier::Common)] == Catch::Approx(10.0F));
  CHECK(manifest.delivered_mass_by_tier[static_cast<std::size_t>(hyperverse::OreTier::Rare)] == Catch::Approx(4.0F));
}

TEST_CASE("cargo manifest default payout favors premium ore sharply") {
  entt::registry registry;
  const entt::entity common = registry.create();
  const entt::entity anomalous = registry.create();
  registry.emplace<hyperverse::MiningResource>(common, hyperverse::MiningResource{.tier = hyperverse::OreTier::Common, .extracted_mass = 20.0F});
  registry.emplace<hyperverse::MiningResource>(anomalous, hyperverse::MiningResource{.tier = hyperverse::OreTier::Anomalous, .extracted_mass = 2.0F});
  hyperverse::CargoManifest manifest;

  const hyperverse::CargoHudSnapshot cargo = hyperverse::update_cargo_manifest(manifest, registry);

  CHECK(cargo.cash == Catch::Approx(140.0F));
  CHECK(cargo.delivered_mass_by_tier[static_cast<std::size_t>(hyperverse::OreTier::Common)] == Catch::Approx(20.0F));
  CHECK(cargo.delivered_mass_by_tier[static_cast<std::size_t>(hyperverse::OreTier::Anomalous)] == Catch::Approx(2.0F));
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

TEST_CASE("cargo boxes inherit ore color tiers from extracted mass buckets") {
  entt::registry registry;
  const entt::entity common = registry.create();
  const entt::entity rare = registry.create();
  registry.emplace<hyperverse::MiningResource>(common, hyperverse::MiningResource{.tier = hyperverse::OreTier::Common, .extracted_mass = 10.0F});
  registry.emplace<hyperverse::MiningResource>(rare, hyperverse::MiningResource{.tier = hyperverse::OreTier::Rare, .extracted_mass = 20.0F});
  hyperverse::CargoManifest manifest;
  (void)hyperverse::update_cargo_manifest(manifest, registry, {.cargo_box_mass = 10.0F});

  (void)hyperverse::sync_cargo_boxes(
    registry,
    manifest,
    {.position = {.x = 1000.0F, .y = 2000.0F}},
    {.box_mass = 10.0F, .box_spacing = 50.0F}
  );

  int common_boxes = 0;
  int rare_boxes = 0;
  for (auto [entity, box] : registry.view<hyperverse::CargoBox>().each()) {
    (void)entity;
    if (box.tier == hyperverse::OreTier::Common) {
      ++common_boxes;
    }
    if (box.tier == hyperverse::OreTier::Rare) {
      ++rare_boxes;
    }
  }

  CHECK(common_boxes == 1);
  CHECK(rare_boxes == 2);
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

TEST_CASE("extraction route is furthest wrapped point from gathering") {
  const hyperverse::CargoEscortRoute route = hyperverse::extraction_route_from_gathering(
    {.x = 1000.0F, .y = 2000.0F},
    {.width = 9000.0F, .height = 9000.0F}
  );

  CHECK(route.gate_position.x == Catch::Approx(5500.0F));
  CHECK(route.gate_position.y == Catch::Approx(6500.0F));
}

TEST_CASE("cargo escort begins extraction when the active train reaches the gate") {
  hyperverse::CargoEscortState escort{.phase = hyperverse::CargoEscortPhase::EscortActive};
  const hyperverse::CargoHudSnapshot cargo{.extraction_authorized = true};

  const hyperverse::CargoEscortHudSnapshot delivered = hyperverse::update_cargo_escort_arrival(
    escort,
    cargo,
    {.gate_distance = 20.0F, .active = true, .gate_reached = true}
  );

  CHECK(escort.phase == hyperverse::CargoEscortPhase::Extracting);
  CHECK(delivered.phase == hyperverse::CargoEscortPhase::Extracting);
  CHECK_FALSE(delivered.cargo_train_active);
  CHECK(delivered.cargo_extracting);
}

TEST_CASE("cargo extraction processes boxes one at a time at the gate") {
  entt::registry registry;
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    first,
    hyperverse::CargoBox{.position = {.x = 1000.0F, .y = 1000.0F}, .index = 0}
  );
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    second,
    hyperverse::CargoBox{.position = {.x = 1000.0F, .y = 1000.0F}, .index = 1}
  );
  hyperverse::CargoEscortState escort{.phase = hyperverse::CargoEscortPhase::Extracting};
  hyperverse::DomainEventBus event_bus;
  int extracted_events = 0;
  int complete_events = 0;
  event_bus.appendListener(hyperverse::DomainEventType::CargoBoxExtracted, [&](const hyperverse::DomainEvent&) { ++extracted_events; });
  event_bus.appendListener(hyperverse::DomainEventType::CargoExtractionComplete, [&](const hyperverse::DomainEvent&) { ++complete_events; });

  const hyperverse::CargoEscortRoute route{.gate_position = {.x = 1000.0F, .y = 1000.0F}};

  hyperverse::CargoExtractionHudSnapshot first_tick = hyperverse::update_cargo_extraction(
    registry,
    escort,
    route,
    {.width = 9000.0F, .height = 9000.0F},
    5.0F,
    &event_bus
  );
  event_bus.process();

  CHECK(first_tick.active_box_index == 0);
  CHECK(registry.get<hyperverse::CargoBox>(first).state == hyperverse::CargoBoxState::Extracted);
  CHECK(registry.get<hyperverse::CargoBox>(second).state != hyperverse::CargoBoxState::Extracted);
  CHECK(extracted_events == 1);
  CHECK(complete_events == 0);

  (void)hyperverse::update_cargo_extraction(
    registry,
    escort,
    route,
    {.width = 9000.0F, .height = 9000.0F},
    5.0F,
    &event_bus
  );
  (void)hyperverse::update_cargo_extraction(
    registry,
    escort,
    route,
    {.width = 9000.0F, .height = 9000.0F},
    0.0F,
    &event_bus
  );
  event_bus.process();

  CHECK(registry.get<hyperverse::CargoBox>(second).state == hyperverse::CargoBoxState::Extracted);
  CHECK(escort.phase == hyperverse::CargoEscortPhase::Complete);
  CHECK(extracted_events == 2);
  CHECK(complete_events == 1);
}

TEST_CASE("cargo extraction stages the whole group near the gate before loading") {
  entt::registry registry;
  const entt::entity first = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    first,
    hyperverse::CargoBox{.position = {.x = 500.0F, .y = 1000.0F}, .index = 0}
  );
  const entt::entity second = registry.create();
  registry.emplace<hyperverse::CargoBox>(
    second,
    hyperverse::CargoBox{.position = {.x = 450.0F, .y = 1000.0F}, .index = 1}
  );
  hyperverse::CargoEscortState escort{.phase = hyperverse::CargoEscortPhase::Extracting};
  const hyperverse::CargoEscortRoute route{.gate_position = {.x = 1000.0F, .y = 1000.0F}};

  const hyperverse::CargoExtractionHudSnapshot hud = hyperverse::update_cargo_extraction(
    registry,
    escort,
    route,
    {.width = 9000.0F, .height = 9000.0F},
    0.5F,
    nullptr,
    {.seconds_per_box = 5.0F, .staging_radius = 40.0F, .staging_spacing = 80.0F, .approach_rate = 2.0F, .max_speed = 100.0F}
  );

  CHECK(hud.active);
  CHECK(hud.extracted_boxes == 0);
  CHECK(registry.get<hyperverse::CargoBox>(first).state == hyperverse::CargoBoxState::GateBound);
  CHECK(registry.get<hyperverse::CargoBox>(second).state == hyperverse::CargoBoxState::GateBound);
  CHECK(registry.get<hyperverse::CargoBox>(first).position.x > 500.0F);
  CHECK(registry.get<hyperverse::CargoBox>(second).position.x > 450.0F);
}

TEST_CASE("burst speed detaches linked cargo train boxes") {
  entt::registry registry;
  const entt::entity linked = registry.create();
  registry.emplace<hyperverse::CargoBox>(linked, hyperverse::CargoBox{.state = hyperverse::CargoBoxState::Linked});
  const entt::entity stolen = registry.create();
  registry.emplace<hyperverse::CargoBox>(stolen, hyperverse::CargoBox{.state = hyperverse::CargoBoxState::Stolen});

  const int detached = hyperverse::detach_linked_cargo(registry, {.x = 120.0F, .y = 0.0F});

  CHECK(detached == 1);
  CHECK(registry.get<hyperverse::CargoBox>(linked).state == hyperverse::CargoBoxState::Detached);
  CHECK(registry.get<hyperverse::CargoBox>(linked).velocity.x == Catch::Approx(120.0F));
  CHECK(registry.get<hyperverse::CargoBox>(stolen).state == hyperverse::CargoBoxState::Stolen);
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
