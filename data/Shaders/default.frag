#define PI 3.14159

// Lighting only make sense if normals are avaialable
#if defined(HAS_NORMALS) || defined(FLAT_SHADING)

struct PointLight
{
    vec3 color;
    float range;
    vec3 positionVS;
    float intensity;
};

struct DirectionalLight
{
    vec3 color;
	float intensity;
	vec3 direction;
};

struct SpotLight
{
    vec3 color;
	float range;
	vec3 positionVS;
	float angleScale;
	vec3 direction;
	float angleOffset;
    float intensity;
};

// These are now defined in C++
// #define MAX_NUM_POINT_LIGHTS 25
// #define MAX_NUM_SPOT_LIGHTS 25
// #define MAX_NUM_DIR_LIGHTS 5

layout (std140) uniform Lights {
    PointLight pointLight[MAX_NUM_POINT_LIGHTS];
    SpotLight spotLight[MAX_NUM_SPOT_LIGHTS];
    DirectionalLight dirLight[MAX_NUM_DIR_LIGHTS];
    int numPointLights;
    int numSpotLights;
    int numDirLights;
};

struct Material
{
    vec4 baseColorFactor;
    sampler2D baseColorTexture;
    float metallicFactor;
    float roughnessFactor; 
    sampler2D metallicRoughnessTexture;
    sampler2D normalTexture;
    float normalScale;
    float occlusionStrength;
    sampler2D occlusionTexture;
};

uniform Material material;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

#endif // HAS_NORMALS || FLAT_SHADING

#ifdef HAS_TEXCOORD
in vec2 texCoords;
#endif // HAS_TEXCOORD

in vec3 surfacePosVS;

#ifdef HAS_NORMALS
    #ifdef HAS_TANGENTS
        in mat3 TBN;
    #else
        in vec3 surfaceNormalVS;
    #endif // HAS_TANGENTS
#endif // HAS_NORMALS

#ifdef HAS_VERTEX_COLORS
in vec4 vertexColor;
#endif // HAS_VERTEX_COLORS

layout(location=0) out vec4 fragColor;
layout(location=1) out float highlight;

void main()
{
    highlight = 1;
#ifdef HAS_NORMALS
    #ifdef HAS_TANGENTS
        mat3 normalizedTBN = mat3(normalize(TBN[0]), normalize(TBN[1]), normalize(TBN[2]));
        vec3 unitNormal = texture(material.normalTexture, texCoords).rgb;
        unitNormal = unitNormal * 2.0 - 1.0;
        unitNormal *= vec3(material.normalScale, material.normalScale, 1.0);
        unitNormal = normalize(normalizedTBN * unitNormal);
    #else
        vec3 unitNormal = normalize(surfaceNormalVS);
    #endif // HAS_TANGENTS
#elif defined(FLAT_SHADING)
    vec3 dxTangent = dFdx(surfacePosVS);
    vec3 dyTangent = dFdy(surfacePosVS);
    vec3 unitNormal = normalize(cross(dxTangent, dyTangent));
#endif // HAS_NORMALS
    
#if defined(HAS_NORMALS) || defined(FLAT_SHADING)

    #ifdef HAS_TEXCOORD
        vec4 baseColor = texture(material.baseColorTexture, texCoords) * material.baseColorFactor;
        vec2 metallicRoughness = texture(material.metallicRoughnessTexture, texCoords).gb * vec2(material.metallicFactor, material.roughnessFactor);
        float occlusion = texture(material.occlusionTexture, texCoords).r * material.occlusionStrength;
    #else
        vec4 baseColor = material.baseColorFactor;
        vec2 metallicRoughness = vec2(material.metallicFactor, material.roughnessFactor);
        float occlusion = material.occlusionStrength;
    #endif // HAS_TEXCOORD
    #ifdef HAS_VERTEX_COLORS
        baseColor = baseColor * vertexColor;
    #endif // HAS_VERTEX_COLORS

    vec3 surfaceToCamera = -normalize(surfacePosVS);
    float metallic = metallicRoughness.x;
    float roughness = metallicRoughness.y;
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, baseColor.rgb, metallic);

    vec3 finalColor = vec3(0.0);

    for (int i = 0; i < numPointLights; i++) {
        // PointLight light = pointLight[i];
        float surfaceToLightDistance = length(pointLight[i].positionVS - surfacePosVS);
        if (surfaceToLightDistance > pointLight[i].range) {
            continue;
        }
        vec3 surfaceToLight = normalize(pointLight[i].positionVS - surfacePosVS);
        vec3 H = normalize(surfaceToCamera + surfaceToLight);
        float attenuation = 1.0 / (surfaceToLightDistance * surfaceToLightDistance);
        vec3 radiance = pointLight[i].color * attenuation * pointLight[i].intensity;
        float NDF = DistributionGGX(unitNormal, H, roughness);  
        float G = GeometrySmith(unitNormal, surfaceToCamera, surfaceToLight, roughness);
        vec3 F = fresnelSchlick(max(dot(H, surfaceToCamera), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(unitNormal, surfaceToCamera), 0.0) * max(dot(unitNormal, surfaceToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);
        vec3 color = geometryTerm * radiance * (kD * baseColor.rgb / PI + specular);
        vec3 ambient = vec3(0.03) * baseColor.rgb * occlusion;
        color += ambient;
        finalColor += color;
    }

    for (int i = 0; i < numSpotLights; i++)
    {
        SpotLight light = spotLight[i];
        float surfaceToLightDistance = length(light.positionVS - surfacePosVS);
        if (surfaceToLightDistance > light.range) {
            continue;
        }
        vec3 surfaceToLight = normalize(light.positionVS - surfacePosVS);
        vec3 H = normalize(surfaceToCamera + surfaceToLight);
        float distanceAttenuation = 1.0 / (surfaceToLightDistance * surfaceToLightDistance);
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
        float cosLightSurfaceAngle = dot(light.direction, -surfaceToLight);
        float angularAttenuation = clamp(cosLightSurfaceAngle * light.angleScale + light.angleOffset, 0.0, 1.0);
        angularAttenuation *= angularAttenuation;
        vec3 radiance = light.color * distanceAttenuation * angularAttenuation * light.intensity;
        float NDF = DistributionGGX(unitNormal, H, roughness);  
        float G = GeometrySmith(unitNormal, surfaceToCamera, surfaceToLight, roughness);
        vec3 F = fresnelSchlick(max(dot(H, surfaceToCamera), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(unitNormal, surfaceToCamera), 0.0) * max(dot(unitNormal, surfaceToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);
        vec3 color = geometryTerm * radiance * (kD * baseColor.rgb / PI + specular);
        vec3 ambient = vec3(0.03) * baseColor.rgb * occlusion;
        color += ambient;
        finalColor += color;
    }

    for (int i = 0; i < numDirLights; i++)
    {
        DirectionalLight light = dirLight[i];
        vec3 surfaceToLight = -light.direction;
        vec3 H = normalize(surfaceToCamera + surfaceToLight);
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
        float cosLightSurfaceAngle = dot(light.direction, surfaceToLight);
        vec3 radiance = light.color * light.intensity;
        float NDF = DistributionGGX(unitNormal, H, roughness);  
        float G = GeometrySmith(unitNormal, surfaceToCamera, surfaceToLight, roughness);
        vec3 F = fresnelSchlick(max(dot(H, surfaceToCamera), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(unitNormal, surfaceToCamera), 0.0) * max(dot(unitNormal, surfaceToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);
        vec3 color = geometryTerm * radiance * (kD * baseColor.rgb / PI + specular);
        vec3 ambient = vec3(0.03) * baseColor.rgb * occlusion;
        color += ambient;
        finalColor += color;
    }

    fragColor = vec4(finalColor, baseColor.a);
#else
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
#endif // HAS_NORMALS

}