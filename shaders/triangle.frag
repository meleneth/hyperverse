#version 450
layout(location = 0) in vec2 in_surface;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

float hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }
float value_noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - (2.0 * f));
  return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), u.x),
             mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0)), u.x), u.y);
}
void main() {
  float coarse = value_noise(in_surface * 8.0);
  float fine = value_noise((in_surface * 29.0) + vec2(17.0, 5.0));
  float strata = sin((in_surface.x * 31.0) + (in_surface.y * 11.0) + (fine * 2.4));
  float mottled = 0.78 + (coarse * 0.22) + (fine * 0.12) + (strata * 0.045);
  float vein = smoothstep(0.82, 1.0, value_noise((in_surface * 13.0) + vec2(3.0, 19.0))) * 0.08;
  out_color = vec4(in_color.rgb * (mottled + vein), in_color.a);
}
