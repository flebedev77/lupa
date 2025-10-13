#version 330 core
out vec4 FragColor;
in vec2 texCoord;
uniform sampler2D textureSampler;

void main()
{
  vec2 transformedCoord = (texCoord * 2)-1;
  if (length(transformedCoord) < 1) {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
  } else {
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
  }
}

  if (lerp(0, 1, length(transformedCoord))) {



/* Opengl coordinate space
           1    
  -1               1
          -1

  Opengl texture space
           0
  0                1
           1
*/


  if (length(transformedCoord) > 1) {
    FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  } else if (length(transformedCoord) > 0.9) {
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
  }
  float solidity = mix(1, 0, (length(transformedCoord)*sharpness)-(sharpness-5));
