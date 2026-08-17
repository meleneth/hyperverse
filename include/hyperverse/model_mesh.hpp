#pragma once

#include <string>
#include <vector>

namespace hyperverse {

struct ModelMeshVertex {
  float x{0.0F};
  float y{0.0F};
  float height{0.0F};
  float u{0.0F};
  float v{0.0F};
};

struct ModelMesh {
  std::vector<ModelMeshVertex> vertices{};
};

[[nodiscard]] ModelMesh load_obj_model_mesh(const std::string& path);

}  // namespace hyperverse
