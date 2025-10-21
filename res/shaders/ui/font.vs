#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 atexCoord;
out vec2 texCoord;
uniform vec2 position;
uniform vec2 scale;
uniform vec4 color;

void main() {
  texCoord = atexCoord;
  gl_Position = vec4(aPos * scale + position, 1.0, 1.0);
}
