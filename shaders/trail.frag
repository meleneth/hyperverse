#version 450
layout(location = 0) in float in_normalized_age;
layout(location = 1) in float in_intensity;
layout(location = 2) in float in_side;
layout(location = 0) out vec4 out_color;
void main() {
  float lateral = clamp(abs(in_side), 0.0, 1.0);
  float soft_edge = 1.0 - smoothstep(0.04, 1.0, lateral);
  float hot_center = pow(1.0 - lateral, 5.0);
  float is_source = in_normalized_age < 0.0 ? 1.0 : 0.0;
  float age = max(in_normalized_age, 0.0);
  float stepped_age = floor(age * 7.0) / 7.0;
  float exposure_band = 0.72 + (0.28 * step(0.5, fract(age * 7.0)));
  float fade = mix(exp(-stepped_age * 3.8) * (1.0 - (age * 0.42)) * exposure_band, 1.0, is_source);
  vec3 warm = mix(vec3(0.90, 0.02, 0.00), vec3(1.00, 0.28, 0.04), clamp(in_intensity, 0.0, 1.0));
  vec3 heated = mix(warm, vec3(1.00, 0.92, 0.58), hot_center * 0.72);
  vec3 source_color = mix(heated, vec3(1.00, 0.98, 0.86), hot_center);
  vec3 color = mix(heated, source_color, is_source);
  float source_boost = mix(1.0, 1.8, is_source);
  float alpha = soft_edge * fade * clamp(in_intensity, 0.0, 1.0);
  out_color = vec4(color * alpha * 1.35 * source_boost, alpha);
}
