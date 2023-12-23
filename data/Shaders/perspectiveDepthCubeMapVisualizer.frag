out vec4 fragColor;

in vec2 texCoords;

uniform samplerCube depthMap;
uniform float nearPlane;
uniform float farPlane;
uniform int face; // 0 = +x, 1 = -x, 2 = +y, 3 = -y, 4 = +z, 5 = -z

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main()
{
    vec3 cubeMapDir;         
    vec2 texCoordsCubeSpace = texCoords * 2.0 - 1.0; // [0, 1] to [-1, 1]
    switch(face)
    {
        case 0: cubeMapDir = vec3(1.0, texCoordsCubeSpace); break;
        case 1: cubeMapDir = vec3(-1.0, texCoordsCubeSpace); break;
        case 2: cubeMapDir = vec3(texCoordsCubeSpace.x, 1.0, texCoordsCubeSpace.y); break;
        case 3: cubeMapDir = vec3(texCoordsCubeSpace.x, -1.0, texCoordsCubeSpace.y); break;
        case 4: cubeMapDir = vec3(texCoordsCubeSpace, 1.0); break;
        case 5: cubeMapDir = vec3(texCoordsCubeSpace, -1.0); break;
    }
    float depthValue = texture(depthMap, cubeMapDir).r;
    fragColor = vec4(vec3(LinearizeDepth(depthValue) / farPlane), 1.0); // perspective
}