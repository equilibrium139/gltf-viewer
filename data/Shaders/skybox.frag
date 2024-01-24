out vec4 fragColor;

in vec3 localPos;
  
uniform samplerCube environmentMap;
uniform float levelOfDetail;
  
void main()
{
    vec3 envColor = textureLod(environmentMap, localPos, levelOfDetail).rgb;  
    fragColor = vec4(envColor, 1.0);
}