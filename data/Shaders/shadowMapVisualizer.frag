out vec4 FragColor;

in vec2 texCoords;

uniform sampler2D depthMap;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    const float near_plane = 0.1;
    const float far_plane = 50.0f;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

void main()
{             
    // float depthValue = texture(depthMap, texCoords).r;
    // FragColor = vec4(depthValue, depthValue, depthValue, 1.0);
    // FragColor = vec4(texCoords.x, texCoords.y, 0.0, 1.0);
    // if (depthValue == 1.0)
    // {
    //     FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    // }
    // else if (depthValue == 0.0)
    // {
    //     FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    // }
    // else 
    // {
    //     FragColor = vec4(0.0, 0.0, 1.0, 1.0);
    // }
    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / 50.0f), 1.0); // perspective
    // FragColor = vec4(vec3(depthValue), 1.0); // orthographics
    vec4 sceneColor = texture(depthMap, texCoords);
    const float exposure = 1.0;
    vec3 tonemappedColor = vec3(1.0) - exp(-sceneColor.rgb * exposure);
    vec3 gammaCorrectedColor = pow(tonemappedColor, vec3(1.0 / 2.2));
    FragColor = vec4(gammaCorrectedColor, sceneColor.a);
}