#include "hyperverse/model_mesh.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace hyperverse {
namespace {

struct Position {
  float x{};
  float y{};
  float z{};
};

struct Uv {
  float u{};
  float v{};
};

struct FaceVertex {
  std::size_t position{};
  std::size_t uv{};
};

[[nodiscard]] FaceVertex parse_face_vertex(std::string_view token) {
  const std::size_t first_slash = token.find('/');
  if (first_slash == std::string_view::npos) throw std::runtime_error("OBJ face is missing UV coordinates");
  const std::size_t second_slash = token.find('/', first_slash + 1U);
  const std::string_view position_text = token.substr(0, first_slash);
  const std::string_view uv_text = token.substr(
    first_slash + 1U,
    (second_slash == std::string_view::npos ? token.size() : second_slash) - first_slash - 1U
  );
  unsigned long position = 0;
  unsigned long uv = 0;
  try {
    position = std::stoul(std::string{position_text});
    uv = std::stoul(std::string{uv_text});
  } catch (const std::exception&) {
    throw std::runtime_error("OBJ face contains an invalid index");
  }
  if (position == 0 || uv == 0) throw std::runtime_error("OBJ indices must be positive");
  return {.position = position - 1U, .uv = uv - 1U};
}

}  // namespace

ModelMesh load_obj_model_mesh(const std::string& path) {
  std::ifstream stream{path};
  if (!stream) throw std::runtime_error("failed to open model: " + path);

  std::vector<Position> positions;
  std::vector<Uv> uvs;
  std::vector<std::array<FaceVertex, 3>> triangles;
  std::string line;
  while (std::getline(stream, line)) {
    std::istringstream row{line};
    std::string kind;
    row >> kind;
    if (kind == "v") {
      Position position{};
      if (!(row >> position.x >> position.y >> position.z)) throw std::runtime_error("invalid OBJ position");
      positions.push_back(position);
    } else if (kind == "vt") {
      Uv uv{};
      if (!(row >> uv.u >> uv.v)) throw std::runtime_error("invalid OBJ UV");
      uvs.push_back(uv);
    } else if (kind == "f") {
      std::vector<FaceVertex> face;
      std::string token;
      while (row >> token) face.push_back(parse_face_vertex(token));
      if (face.size() < 3U) throw std::runtime_error("OBJ face has fewer than three vertices");
      for (std::size_t index = 1; index + 1U < face.size(); ++index) {
        triangles.push_back({face[0], face[index], face[index + 1U]});
      }
    }
  }
  if (positions.empty() || uvs.empty() || triangles.empty()) throw std::runtime_error("OBJ model has no renderable UV triangles");

  float min_x = positions.front().x;
  float max_x = positions.front().x;
  float min_y = positions.front().z;
  float max_y = positions.front().z;
  for (const Position& position : positions) {
    min_x = std::min(min_x, position.x);
    max_x = std::max(max_x, position.x);
    min_y = std::min(min_y, position.z);
    max_y = std::max(max_y, position.z);
  }
  const float center_x = (min_x + max_x) * 0.5F;
  const float center_y = (min_y + max_y) * 0.5F;
  const float inverse_span = 1.0F / std::max({max_x - min_x, max_y - min_y, 0.0001F});

  ModelMesh mesh;
  mesh.vertices.reserve(triangles.size() * 3U);
  for (const auto& triangle : triangles) {
    for (const FaceVertex& face_vertex : triangle) {
      if (face_vertex.position >= positions.size() || face_vertex.uv >= uvs.size()) {
        throw std::runtime_error("OBJ face index is out of range");
      }
      const Position& position = positions[face_vertex.position];
      const Uv& uv = uvs[face_vertex.uv];
      mesh.vertices.push_back({
        .x = (position.x - center_x) * inverse_span,
        .y = (position.z - center_y) * inverse_span,
        .height = position.y,
        .u = uv.u,
        .v = 1.0F - uv.v,
      });
    }
  }
  return mesh;
}

}  // namespace hyperverse
