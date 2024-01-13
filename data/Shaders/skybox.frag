out vec4 fragColor;

in vec3 localPos;
  
uniform samplerCube environmentMap;
  
void main()
{
    vec3 envColor = texture(environmentMap, localPos).rgb;  
    fragColor = vec4(envColor, 1.0);
}