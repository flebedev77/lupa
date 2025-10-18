#version 330 core
out vec4 FragColor;
in vec2 texCoord;
uniform sampler2D textureSampler;
uniform float zoom;
uniform vec2 mousepos;

void main() {
  // FragColor = texture(textureSampler, (texCoord * zoom - zoom / 2) + mousepos / zoom);
  FragColor = texture(textureSampler, texCoord);
}
