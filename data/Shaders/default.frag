// Lights only make sense if normals are avaialable
#ifdef HAS_NORMALS

struct PointLight
{
    vec3 positionVS;
    vec3 color;
};

uniform PointLight pointLight;

#endif // HAS_NORMALS


in vec3 surfacePosVS;

#ifdef HAS_NORMALS
in vec3 surfaceNormalVS;
#endif // HAS_NORMALS

out vec4 color;

void main()
{
#ifdef HAS_NORMALS
    vec3 unitNormal = normalize(surfaceNormalVS);
    vec3 surfaceToLight = normalize(pointLight.positionVS - surfacePosVS);
    vec3 surfaceToCamera = -normalize(surfacePosVS);
    float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);
    color = geometryTerm * vec4(1.0, 0.0, 0.0, 1.0);
#else
    color = vec4(1.0, 0.0, 0.0, 1.0);
#endif // HAS_NORMALS

}