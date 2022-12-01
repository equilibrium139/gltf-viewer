#version 330 core
layout(location = 0) in vec3 aBasePos;
layout(location = 1) in vec3 aMorphBaseDifference1;
layout(location = 2) in vec3 aMorphBaseDifference2;

uniform mat4 mvp;
uniform float morph1Weight; 
uniform float morph2Weight;
void main()
{
    // vec3 morphDifferenceInterpolated = aMorphBaseDifference1 * morphWeight + aMorphBaseDifference2 * (1.0 - morphWeight);
    vec3 morphed = aBasePos + 
                            morph1Weight * aMorphBaseDifference1 +
                            morph2Weight * aMorphBaseDifference2;
    gl_Position = mvp * vec4(morphed, 1.0);
}