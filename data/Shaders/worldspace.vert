layout(location = 0) in vec3 aBasePos;

#ifdef HAS_JOINTS
layout(location = 3) in vec4 aWeights;
layout(location = 4) in uint aJoints;
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
layout(location = 5) in vec3 aMorphBasePosDifference1;
layout(location = 6) in vec3 aMorphBasePosDifference2;
#endif // HAS_MORPH_TARGETS

uniform mat4 world;

#ifdef HAS_JOINTS
uniform mat4 skinningMatrices[128];
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
uniform float morph1Weight; 
uniform float morph2Weight;
#endif // HAS_MORPH_TARGETS

void main()
{
    mat4 skinningMatrix = mat4(1.0); // TODO: move this into ifdef?
    vec3 worldPosition = aBasePos;

#ifdef HAS_JOINTS
    vec4 modelSpaceVertex = vec4(surfacePosVS, 1.0);
    skinningMatrix = aWeights.x * skinningMatrices[aJoints & 0xFFu] +
                  aWeights.y * skinningMatrices[(aJoints >> 8) & 0xFFu] +
                  aWeights.z * skinningMatrices[(aJoints >> 16) & 0xFFu] +
                  aWeights.w * skinningMatrices[(aJoints >> 24) & 0xFFu];
    worldPosition = vec3(skinningMatrix * modelSpaceVertex);
#endif // HAS_JOINTS

#ifdef HAS_MORPH_TARGETS
    worldPosition += morph1Weight * aMorphBasePosDifference1 +
                    morph2Weight * aMorphBasePosDifference2;
#endif // HAS_MORPH_TARGETS

    gl_Position = world * vec4(worldPosition, 1.0);
}