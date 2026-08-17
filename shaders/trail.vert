#version 450
layout(location = 0) in vec2 in_position;
layout(location = 1) in float in_normalized_age;
layout(location = 2) in float in_intensity;
layout(location = 3) in float in_side;
layout(location = 0) out float out_normalized_age;
layout(location = 1) out float out_intensity;
layout(location = 2) out float out_side;
void main() {
  gl_Position = vec4(in_position, 0.0, 1.0);
  out_normalized_age = in_normalized_age;
  out_intensity = in_intensity;
  out_side = in_side;
}
