#version 330

uniform mat4 ciModelViewProjection;

in vec4 ciPosition;

in vec4 ciColor;
out lowp vec4 Color;

void main() {
  vec4 clipSpacePos = ciModelViewProjection * ciPosition;
  clipSpacePos.z -= 0.005;
  gl_Position = clipSpacePos;
  Color = ciColor;
}
