#version 450

layout(push_constant) uniform LinePush {
  vec4 endpoints;
  vec4 color;
} line;

layout(location = 0) out vec4 out_color;

void main() {
  vec2 position = gl_VertexIndex == 0 ? line.endpoints.xy : line.endpoints.zw;
  gl_Position = vec4(position, 0.0, 1.0);
  out_color = line.color;
}
