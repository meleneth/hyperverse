#version 450

layout(push_constant) uniform SpritePush {
  vec2 center;
  vec2 half_size;
} sprite;

layout(location = 0) out vec2 out_uv;

const vec2 positions[6] = vec2[](
  vec2(-1.0, -1.0),
  vec2( 1.0, -1.0),
  vec2( 1.0,  1.0),
  vec2(-1.0, -1.0),
  vec2( 1.0,  1.0),
  vec2(-1.0,  1.0)
);

const vec2 uvs[6] = vec2[](
  vec2(0.0, 1.0),
  vec2(1.0, 1.0),
  vec2(1.0, 0.0),
  vec2(0.0, 1.0),
  vec2(1.0, 0.0),
  vec2(0.0, 0.0)
);

void main() {
  vec2 position = sprite.center + (positions[gl_VertexIndex] * sprite.half_size);
  gl_Position = vec4(position, 0.0, 1.0);
  out_uv = uvs[gl_VertexIndex];
}
