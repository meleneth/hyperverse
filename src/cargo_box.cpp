#include "hyperverse/cargo_box.hpp"

namespace hyperverse {

int sync_cargo_boxes(
  entt::registry& registry,
  const CargoManifest& manifest,
  const ExtractionSite& extraction_site,
  const CargoBoxTuning& tuning
) {
  int existing_boxes = 0;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    box.index = existing_boxes;
    box.mass = tuning.box_mass;
    box.position = {.x = extraction_site.position.x + (static_cast<float>(existing_boxes) * tuning.box_spacing), .y = extraction_site.position.y};
    ++existing_boxes;
  }

  while (existing_boxes < manifest.cargo_boxes) {
    const entt::entity box_entity = registry.create();
    registry.emplace<CargoBox>(
      box_entity,
      CargoBox{
        .position = {.x = extraction_site.position.x + (static_cast<float>(existing_boxes) * tuning.box_spacing), .y = extraction_site.position.y},
        .mass = tuning.box_mass,
        .index = existing_boxes,
      }
    );
    ++existing_boxes;
  }

  return existing_boxes;
}

}  // namespace hyperverse
