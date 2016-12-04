#version 330

uniform sampler2D uTex0;
uniform float distortionPow;

in vec2 TexCoord;
in vec4 Color;
in vec3 Normal;

out vec4 FragColor;

void main() {
  vec2 center = vec2(0.5);

  vec2 fromCenter = center - TexCoord;
  float dist = pow(length(fromCenter), distortionPow);
  vec2 newTexCoord = center + dist * normalize(fromCenter);

  FragColor = texture(uTex0, newTexCoord);
}
