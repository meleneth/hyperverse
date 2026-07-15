#version 450

layout(push_constant) uniform SpritePush {
  vec4 transform;
  vec4 rotation;
  vec4 tint;
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
  vec2(0.0, 0.0),
  vec2(1.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 1.0)
);

void main() {
  float sine = sin(sprite.rotation.x);
  float cosine = cos(sprite.rotation.x);
  mat2 rotate = mat2(cosine, sine, -sine, cosine);
  float aspect = sprite.rotation.y;
  vec2 local = positions[gl_VertexIndex] * sprite.transform.zw;
  local.x *= aspect;
  local = rotate * local;
  local.x /= aspect;
  vec2 position = sprite.transform.xy + local;
  gl_Position = vec4(position, 0.0, 1.0);
  out_uv = uvs[gl_VertexIndex];
}
