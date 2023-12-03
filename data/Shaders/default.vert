layout(location = 0) in vec3 aBasePos;

#ifdef HAS_TEXCOORD
layout(location = 1) in vec2 aTexcoords;
#endif // HAS_TEXCOORD

#ifdef HAS_NORMALS
layout(location = 2) in vec3 aBaseNormal;
#endif // HAS_NORMALS

#ifdef HAS_JOINTS
layout(location = 3) in vec4 aWeights;
layout(location = 4) in uint aJoints;
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
layout(location = 5) in vec3 aMorphBasePosDifference1;
layout(location = 6) in vec3 aMorphBasePosDifference2;
    #ifdef HAS_NORMALS
        layout(location = 7) in vec3 aMorphBaseNormalDifference1;
        layout(location = 8) in vec3 aMorphBaseNormalDifference2;
    #endif // HAS_NORMALS
    #ifdef HAS_TANGENTS
        layout(location = 10) in vec3 aMorphBaseTangentDifference1;
        layout(location = 11) in vec3 aMorphBaseTangentDifference2;
    #endif // HAS_TANGENTS
#endif // HAS_MORPH_TARGETS

#ifdef HAS_TANGENTS
layout(location = 9) in vec4 aBaseTangent;
#endif // HAS_TANGENTS

#ifdef HAS_VERTEX_COLORS
    layout(location=12) in vec4 aVertexColor;
#endif // HAS_VERTEX_COLORS

uniform mat4 world;
uniform mat4 view;
uniform mat4 projection;

#ifdef HAS_NORMALS
uniform mat3 normalMatrixVS;
#endif // HAS_NORMALS

#ifdef HAS_JOINTS
uniform mat4 skinningMatrices[128];
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
uniform float morph1Weight; 
uniform float morph2Weight;
#endif // HAS_MORPH_TARGETS

#if defined(HAS_NORMALS) || defined(FLAT_SHADING)
uniform mat4 worldToShadowMapUVSpace[MAX_NUM_SPOT_LIGHTS + MAX_NUM_DIR_LIGHTS];
#endif

out VS_OUT {

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
    // TODO: ridiculous name, consider renaming
    vec4 surfacePosShadowMapUVSpace[MAX_NUM_SPOT_LIGHTS + MAX_NUM_DIR_LIGHTS];
#endif 

} vsOut;

void main()
{
    vec3 surfacePos = aBasePos;

#ifdef HAS_NORMALS
    vec3 normal = aBaseNormal;
#endif // HAS_NORMALS

    mat4 skinningMatrix = mat4(1.0); // TODO: move this into ifdef?

// TODO: make sure skeletal animation is independent of morph target animation
#ifdef HAS_JOINTS
    vec4 modelSpaceVertex = vec4(surfacePos, 1.0);
    skinningMatrix = aWeights.x * skinningMatrices[aJoints & 0xFFu] +
                  aWeights.y * skinningMatrices[(aJoints >> 8) & 0xFFu] +
                  aWeights.z * skinningMatrices[(aJoints >> 16) & 0xFFu] +
                  aWeights.w * skinningMatrices[(aJoints >> 24) & 0xFFu];
    surfacePos = vec3(skinningMatrix * modelSpaceVertex);
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
    surfacePos += morph1Weight * aMorphBasePosDifference1 +
                    morph2Weight * aMorphBasePosDifference2;
    #ifdef HAS_NORMALS               
        normal += morph1Weight * aMorphBaseNormalDifference1 +
                  morph2Weight * aMorphBaseNormalDifference2;
    #endif // HAS_NORMALS
#endif // HAS_MORPH_TARGETS

    vec4 surfacePosWS = world * vec4(surfacePos, 1.0);

#if defined(HAS_NORMALS) || defined(FLAT_SHADING)
    for (int i = 0; i < MAX_NUM_DIR_LIGHTS + MAX_NUM_SPOT_LIGHTS; i++)
    {
        vsOut.surfacePosShadowMapUVSpace[i] = worldToShadowMapUVSpace[i] * surfacePosWS;
    }
#endif 

    vsOut.surfacePosVS = vec3(view * surfacePosWS);


#ifdef HAS_NORMALS
    mat3 finalNormalMatrix = normalMatrixVS;
    #ifdef HAS_JOINTS
        // take into account skinning matrix transformation
        finalNormalMatrix = finalNormalMatrix * transpose(inverse(mat3(skinningMatrix)));
    #endif
    normal = normalize(finalNormalMatrix * normal);

    #ifdef HAS_TANGENTS
        vsOut.TBN[0] = vec3(aBaseTangent);
        #ifdef HAS_MORPH_TARGETS
        vsOut.TBN[0] += morph1Weight * aMorphBaseTangentDifference1 +
                morph2Weight * aMorphBaseTangentDifference2;
        #endif
        vsOut.TBN[0] = finalNormalMatrix * vsOut.TBN[0];
        vsOut.TBN[1] = cross(normal, vsOut.TBN[0]) * aBaseTangent.w; // w (-1 or 1) determines bitangent direction
        vsOut.TBN[2] = normal;
    #else
        vsOut.surfaceNormalVS = normal;
    #endif // HAS_TANGENTS
#endif // HAS_NORMALS

#ifdef HAS_TEXCOORD
    vsOut.texCoords = aTexcoords;
#endif

#ifdef HAS_VERTEX_COLORS
        vsOut.vertexColor = aVertexColor;
#endif // HAS_VERTEX_COLORS

    gl_Position = projection * vec4(vsOut.surfacePosVS, 1.0);
}