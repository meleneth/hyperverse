#version 450

layout(set = 0, binding = 0) uniform sampler2D sprite_texture;

layout(push_constant) uniform SpritePush {
  vec4 transform;
  vec4 rotation;
  vec4 tint;
} sprite;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
  out_color = texture(sprite_texture, in_uv) * sprite.tint;
}
