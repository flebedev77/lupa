#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 atexCoord;
out vec2 texCoord;

uniform vec2 pos;
uniform vec2 scale;

void main()
{
  gl_Position = vec4(aPos.x * scale.x + pos.x, aPos.y * scale.y + pos.y, 1.0, 1.0);
  texCoord = atexCoord;
}
