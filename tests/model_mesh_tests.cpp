#include "test_common.hpp"

#include "hyperverse/model_mesh.hpp"

#include <algorithm>

TEST_CASE("drone model loads normalized textured triangles") {
  const hyperverse::ModelMesh mesh = hyperverse::load_obj_model_mesh("assets/models/drone/drone.obj");

  REQUIRE_FALSE(mesh.vertices.empty());
  CHECK((mesh.vertices.size() % 3U) == 0U);
  CHECK(mesh.vertices.size() > 900U);
  CHECK(std::ranges::all_of(mesh.vertices, [](const hyperverse::ModelMeshVertex& vertex) {
    return vertex.x >= -0.51F && vertex.x <= 0.51F && vertex.y >= -0.51F && vertex.y <= 0.51F &&
      vertex.u >= 0.0F && vertex.u <= 1.0F && vertex.v >= 0.0F && vertex.v <= 1.0F;
  }));
}
