// Lighting only make sense if normals are avaialable
#if defined(HAS_NORMALS) || defined(FLAT_SHADING)

    struct PointLight
    {
        vec3 color;
        float range;

        vec3 positionVS;
        float intensity;

        // scale and offset used to convert depths for depth map comparisons 
        // equal to 3rd and 4th values in 3rd column of perspective matrix respectively
        float depthScale;
        float depthOffset;
        float shadowMappingBias;
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

    struct DirectionalLight
    {
        vec3 color;
        float intensity;
        
        vec3 direction;
    };

    // These are now defined in C++
    // #define MAX_NUM_POINT_LIGHTS 25
    // #define MAX_NUM_SPOT_LIGHTS 25
    // #define MAX_NUM_DIR_LIGHTS 5

    layout (std140, binding=1) uniform Lights {
        PointLight pointLight[MAX_NUM_POINT_LIGHTS];
        SpotLight spotLight[MAX_NUM_SPOT_LIGHTS];
        DirectionalLight dirLight[MAX_NUM_DIR_LIGHTS];
        int numPointLights;
        int numSpotLights;
        int numDirLights;
    };

    uniform samplerCube irradianceMap;
    uniform samplerCube prefilterMap;
    uniform sampler2D brdfLUT;  

    // TODO: change these to array textures
    uniform samplerCubeShadow depthCubemaps[MAX_NUM_POINT_LIGHTS];
    // Spot light depth maps assumed to start at index 0, directional light depth maps assumed at index MAX_NUM_SPOT_LIGHTS
    uniform sampler2DShadow depthMaps[MAX_NUM_SPOT_LIGHTS + MAX_NUM_DIR_LIGHTS];
    uniform mat4 viewToWorld;

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

    vec3 FresnelSchlick(float cosTheta, vec3 F0)
    {
        // https://www.desmos.com/calculator/s4vkjimp63
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }  

    vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
    {
        return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }  

#endif // HAS_NORMALS || FLAT_SHADING

in VS_OUT {
    vec3 surfacePosVS;

    #ifdef HAS_NORMALS
        #ifdef HAS_TANGENTS
            mat3 TBN;
        #else
            vec3 surfaceNormalVS;
        #endif // HAS_TANGENTS
    #endif // HAS_NORMALS

    #ifdef HAS_TEXCOORD
    vec2 texCoords;
    #endif // HAS_TEXCOORD

    #ifdef HAS_VERTEX_COLORS
    vec4 vertexColor;
    #endif // HAS_VERTEX_COLORS

#if defined(HAS_NORMALS) || defined(FLAT_SHADING)
    vec4 surfacePosShadowMapUVSpace[MAX_NUM_SPOT_LIGHTS + MAX_NUM_DIR_LIGHTS];
#endif

} fsIn;

layout(location=0) out vec4 fragColor;

void main()
{
#ifdef HAS_NORMALS
    #ifdef HAS_TANGENTS
        mat3 normalizedTBN = mat3(normalize(fsIn.TBN[0]), normalize(fsIn.TBN[1]), normalize(fsIn.TBN[2]));
        vec3 unitNormal = texture(material.normalTexture, fsIn.texCoords).rgb;
        unitNormal = unitNormal * 2.0 - 1.0;
        unitNormal *= vec3(material.normalScale, material.normalScale, 1.0);
        unitNormal = normalize(normalizedTBN * unitNormal);
    #else
        vec3 unitNormal = normalize(fsIn.surfaceNormalVS);
    #endif // HAS_TANGENTS
#elif defined(FLAT_SHADING)
    vec3 dxTangent = dFdx(fsIn.surfacePosVS);
    vec3 dyTangent = dFdy(fsIn.surfacePosVS);
    vec3 unitNormal = normalize(cross(dxTangent, dyTangent));
#endif // HAS_NORMALS
    
#if defined(HAS_NORMALS) || defined(FLAT_SHADING)

    #ifdef HAS_TEXCOORD
        vec4 baseColor = texture(material.baseColorTexture, fsIn.texCoords) * material.baseColorFactor;
        vec2 metallicRoughness = texture(material.metallicRoughnessTexture, fsIn.texCoords).bg * vec2(material.metallicFactor, material.roughnessFactor);
        float occlusion = texture(material.occlusionTexture, fsIn.texCoords).r * material.occlusionStrength;
    #else
        vec4 baseColor = material.baseColorFactor;
        vec2 metallicRoughness = vec2(material.metallicFactor, material.roughnessFactor);
        float occlusion = material.occlusionStrength;
    #endif // HAS_TEXCOORD
    #ifdef HAS_VERTEX_COLORS
        baseColor = baseColor * fsIn.vertexColor;
    #endif // HAS_VERTEX_COLORS

    vec3 surfaceToCamera = -normalize(fsIn.surfacePosVS);
    float metallic = metallicRoughness.x;
    float roughness = metallicRoughness.y;
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, baseColor.rgb, metallic);

    vec3 finalColor = vec3(0.0);

    for (int i = 0; i < numPointLights; i++) {
        PointLight light = pointLight[i];
        float surfaceToLightDistance = length(light.positionVS - fsIn.surfacePosVS);
        if (surfaceToLightDistance > light.range) {
            continue;
        }
        vec3 surfaceToLight = normalize(light.positionVS - fsIn.surfacePosVS);
        vec3 H = normalize(surfaceToCamera + surfaceToLight);
        float attenuation = 1.0 / (surfaceToLightDistance * surfaceToLightDistance);
        vec3 radiance = light.color * attenuation * light.intensity;
        float NDF = DistributionGGX(unitNormal, H, roughness);  
        float G = GeometrySmith(unitNormal, surfaceToCamera, surfaceToLight, roughness);
        vec3 F = FresnelSchlick(max(dot(H, surfaceToCamera), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(unitNormal, surfaceToCamera), 0.0) * max(dot(unitNormal, surfaceToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);

        vec3 lightToSurfaceWS = (vec3(viewToWorld * vec4((fsIn.surfacePosVS - light.positionVS), 0.0)));
        
        // Intuition for this: assume x coordinate is largest magnitude coordinate in the vector and it's positive. 
        // This means we will sample from +x face of the cubemap. The depths in the +x face of the cubemap are generated using
        // a camera looking down the world space +x axis, so they are simply the world space x offset from the point light.
        float lightToSurfaceDepth = max(max(abs(lightToSurfaceWS.x), abs(lightToSurfaceWS.y)), abs(lightToSurfaceWS.z));

        lightToSurfaceDepth = (light.depthScale * lightToSurfaceDepth + light.depthOffset) / lightToSurfaceDepth; // [-1, 1]
        lightToSurfaceDepth = (lightToSurfaceDepth + 1.0) * 0.5; // [0, 1]
        vec4 cubeMapCoord = vec4(lightToSurfaceWS, lightToSurfaceDepth - light.shadowMappingBias);
        float shadow = texture(depthCubemaps[i], cubeMapCoord);

        vec3 color = geometryTerm * radiance * shadow * (kD * baseColor.rgb / PI + specular);
        finalColor += color;
    }

    for (int i = 0; i < numSpotLights; i++)
    {
        SpotLight light = spotLight[i];
        float surfaceToLightDistance = length(light.positionVS - fsIn.surfacePosVS);
        if (surfaceToLightDistance > light.range) {
            continue;
        }
        vec3 surfaceToLight = normalize(light.positionVS - fsIn.surfacePosVS);
        vec3 H = normalize(surfaceToCamera + surfaceToLight);
        float distanceAttenuation = 1.0 / (surfaceToLightDistance * surfaceToLightDistance);
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
        float cosLightSurfaceAngle = dot(light.direction, -surfaceToLight);
        float angularAttenuation = clamp(cosLightSurfaceAngle * light.angleScale + light.angleOffset, 0.0, 1.0);
        angularAttenuation *= angularAttenuation;
        vec3 radiance = light.color * distanceAttenuation * angularAttenuation * light.intensity;
        float NDF = DistributionGGX(unitNormal, H, roughness);  
        float G = GeometrySmith(unitNormal, surfaceToCamera, surfaceToLight, roughness);
        vec3 F = FresnelSchlick(max(dot(H, surfaceToCamera), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(unitNormal, surfaceToCamera), 0.0) * max(dot(unitNormal, surfaceToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);
        float shadow = textureProj(depthMaps[i], fsIn.surfacePosShadowMapUVSpace[i]);
        vec3 color = geometryTerm * radiance * shadow * (kD * baseColor.rgb / PI + specular);
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
        vec3 F = FresnelSchlick(max(dot(H, surfaceToCamera), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(unitNormal, surfaceToCamera), 0.0) * max(dot(unitNormal, surfaceToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        float geometryTerm = max(dot(surfaceToLight, unitNormal), 0.0);
        float shadow = textureProj(depthMaps[i + MAX_NUM_SPOT_LIGHTS], fsIn.surfacePosShadowMapUVSpace[i + MAX_NUM_SPOT_LIGHTS]);
        vec3 color = geometryTerm * radiance * shadow * (kD * baseColor.rgb / PI + specular);
        finalColor += color;
    }

    // ambient lighting using irradiance map
    vec3 F = FresnelSchlickRoughness(max(dot(unitNormal, surfaceToCamera), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec3 normalWS = vec3(viewToWorld * vec4(unitNormal, 0.0));
    vec3 irradiance = texture(irradianceMap, normalWS).rgb;
    vec3 diffuse = irradiance * baseColor.rgb;

    vec3 R = reflect(-surfaceToCamera, unitNormal);   
    vec3 reflectionWS = vec3(viewToWorld * vec4(R, 0.0)); // convert to world space so we can properly sample 
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, reflectionWS,  roughness * MAX_REFLECTION_LOD).rgb;    

    vec2 envBRDF = texture(brdfLUT, vec2(max(dot(unitNormal, surfaceToCamera), 0.0), roughness)).rg;
    vec3 indirectSpecular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD * diffuse + indirectSpecular) * occlusion;

    finalColor += ambient;

    fragColor = vec4(finalColor, baseColor.a);
    // fragColor = vec4(finalColor, 1.0);
#else
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
#endif // HAS_NORMALS

}