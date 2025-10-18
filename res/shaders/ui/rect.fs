#version 330 core
out vec4 FragColor;
in vec2 texCoord;

uniform vec4 color;

void main() {
  // FragColor = vec4(0, 1, 1, 1);
  FragColor = color;
} 
