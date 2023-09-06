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

uniform mat4 modelView;
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

out vec3 surfacePosVS;

#ifdef HAS_NORMALS
    #ifdef HAS_TANGENTS
        out mat3 TBN;
    #else
        out vec3 surfaceNormalVS;
    #endif // HAS_TANGENTS
#endif // HAS_NORMALS

#ifdef HAS_TEXCOORD
out vec2 texCoords;
#endif // HAS_TEXCOORD

#ifdef HAS_VERTEX_COLORS
out vec4 vertexColor;
#endif // HAS_VERTEX_COLORS

void main()
{
    surfacePosVS = aBasePos;

#ifdef HAS_NORMALS
    vec3 normal = aBaseNormal;
#endif // HAS_NORMALS

    mat4 skinningMatrix = mat4(1.0); // TODO: move this into ifdef?

// TODO: make sure skeletal animation is independent of morph target animation
#ifdef HAS_JOINTS
    vec4 modelSpaceVertex = vec4(surfacePosVS, 1.0);
    skinningMatrix = aWeights.x * skinningMatrices[aJoints & 0xFFu] +
                  aWeights.y * skinningMatrices[(aJoints >> 8) & 0xFFu] +
                  aWeights.z * skinningMatrices[(aJoints >> 16) & 0xFFu] +
                  aWeights.w * skinningMatrices[(aJoints >> 24) & 0xFFu];
    surfacePosVS = vec3(skinningMatrix * modelSpaceVertex);
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
    surfacePosVS += morph1Weight * aMorphBasePosDifference1 +
                    morph2Weight * aMorphBasePosDifference2;
    #ifdef HAS_NORMALS               
        normal += morph1Weight * aMorphBaseNormalDifference1 +
                  morph2Weight * aMorphBaseNormalDifference2;
    #endif // HAS_NORMALS
#endif // HAS_MORPH_TARGETS

    surfacePosVS = vec3(modelView * vec4(surfacePosVS, 1.0));


#ifdef HAS_NORMALS
    mat3 finalNormalMatrix = normalMatrixVS;
    #ifdef HAS_JOINTS
        // take into account skinning matrix transformation
        finalNormalMatrix = finalNormalMatrix * transpose(inverse(mat3(skinningMatrix)));
    #endif
    normal = normalize(finalNormalMatrix * normal);

    #ifdef HAS_TANGENTS
        TBN[0] = vec3(aBaseTangent);
        #ifdef HAS_MORPH_TARGETS
        TBN[0] += morph1Weight * aMorphBaseTangentDifference1 +
                morph2Weight * aMorphBaseTangentDifference2;
        #endif
        TBN[0] = finalNormalMatrix * TBN[0];
        TBN[1] = cross(normal, TBN[0]) * aBaseTangent.w; // w (-1 or 1) determines bitangent direction
        TBN[2] = normal;
    #else
        surfaceNormalVS = normal;
    #endif // HAS_TANGENTS
#endif // HAS_NORMALS

#ifdef HAS_TEXCOORD
    texCoords = aTexcoords;
#endif

#ifdef HAS_VERTEX_COLORS
        vertexColor = aVertexColor;
#endif // HAS_VERTEX_COLORS

    gl_Position = projection * vec4(surfacePosVS, 1.0);
}