#version 330 core
out vec4 FragColor;
in vec2 texCoord;

uniform sampler2D textureSampler;

uniform vec2 pos;
uniform vec2 scale;

void main() {
  vec2 transformedCoord = (texCoord * 2)-1;

  vec2 transformedPos = (pos + 1) / 2;
  transformedPos.y *= -1;

  vec2 transformedSamplerCoord = texCoord * scale + transformedPos;
  transformedSamplerCoord -= scale / 2;

  vec4 col = texture(textureSampler, transformedSamplerCoord + (length(transformedCoord) * length(transformedCoord))*0.015);

  float thickness = 0.03;
  float fade = 0.01;
  float circle = smoothstep(0.9, 0.9 + fade, length(transformedCoord));
  float circleOuter = smoothstep(0.9 + thickness, 0.9 + thickness + fade, length(transformedCoord));
  col = mix(col, vec4(0, 0, 0, 1), circle);
  col = mix(col, vec4(0, 0, 0, 0), circleOuter);
  FragColor = col;
}
