// Used to transform vertices for things like shadow maps where shading related geometry (normals, tangents, vertex color, etc) is irrelevant
layout(location = 0) in vec3 aBasePos;

#ifdef HAS_JOINTS
layout(location = 3) in vec4 aWeights;
layout(location = 4) in uint aJoints;
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
layout(location = 5) in vec3 aMorphBasePosDifference1;
layout(location = 6) in vec3 aMorphBasePosDifference2;
#endif // HAS_MORPH_TARGETS

uniform mat4 transform;

#ifdef HAS_JOINTS
uniform mat4 skinningMatrices[128];
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
uniform float morph1Weight; 
uniform float morph2Weight;
#endif // HAS_MORPH_TARGETS

void main()
{
    vec3 modelPos = aBasePos;

#ifdef HAS_JOINTS
    vec4 modelSpaceVertex = vec4(modelPos, 1.0);
    mat4 skinningMatrix = aWeights.x * skinningMatrices[aJoints & 0xFFu] +
                  aWeights.y * skinningMatrices[(aJoints >> 8) & 0xFFu] +
                  aWeights.z * skinningMatrices[(aJoints >> 16) & 0xFFu] +
                  aWeights.w * skinningMatrices[(aJoints >> 24) & 0xFFu];
    modelPos = vec3(skinningMatrix * modelSpaceVertex);
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
    modelPos += morph1Weight * aMorphBasePosDifference1 +
                    morph2Weight * aMorphBasePosDifference2;
#endif // HAS_MORPH_TARGETS

    gl_Position = transform * vec4(modelPos, 1.0);
}