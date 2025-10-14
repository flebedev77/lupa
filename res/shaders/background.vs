#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 atexCoord;
out vec2 texCoord;

uniform float zoom;
uniform vec2 mousepos;
uniform vec2 scalePivot;

void main()
{
  vec2 pos = (aPos + scalePivot) * zoom;
  gl_Position = vec4(pos, 1.0, 1.0);

  texCoord = atexCoord;
}
