#version 330 core
out vec4 FragColor;
in vec2 texCoord;

uniform sampler2D textureSampler;

uniform vec2 pos;
uniform vec2 scale;
uniform vec2 screenSize;
uniform float aspectRatio;
uniform float zoom;
uniform vec3 borderColor;

void main() {
  vec2 transformedCoord = (texCoord * 2)-1;

  vec2 transformedPos = (pos + 1) / 2;
  transformedPos.y *= -1;

  vec2 transformedSamplerCoord = texCoord * scale + transformedPos;
  transformedSamplerCoord -= scale / 2;

  float tl = length(transformedCoord);
  tl = tl * tl;
  tl = tl * tl;
  tl = tl * tl;
  tl = tl * tl;
  // vec2 finalSampler = transformedSamplerCoord + (transformedCoord * tl) * 0.01;
  vec2 finalSampler = transformedCoord * (tl + zoom) * 0.01;
  finalSampler.y *= aspectRatio;
  finalSampler = transformedSamplerCoord - finalSampler;
  vec4 col = texture(textureSampler, finalSampler);
  // vec2 samplerDebug = texCoord + (transformedCoord * length(transformedCoord) * length(transformedCoord) * length(transformedCoord) * length(transformedCoord)) * 0.2;
  // col = vec4(samplerDebug * 0.3, 0, 1);

  float thickness = 0;
  float fade = 0.01;
  // float thickness = 0.01;
  // float fade = 0.01;
  float begincircle = (1 - thickness) - fade;
  float circle = smoothstep(begincircle, begincircle + fade, length(transformedCoord));
  float circleOuter = smoothstep(begincircle + thickness, 1, length(transformedCoord));
  // col = mix(col, vec4(borderColor, 1), circle); // Border
  col = mix(col, vec4(0, 0, 0, 0), circleOuter); // End the border color with transparent
  FragColor = col;
}
