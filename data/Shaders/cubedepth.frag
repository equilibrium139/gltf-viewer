uniform float farPlane;
uniform vec3 lightPosWS;

in vec4 surfacePosWS;

void main()
{
    // float distance = length(surfacePosWS.xyz - lightPosWS);
    float depth = abs(surfacePosWS.z - lightPosWS.z);
    gl_FragDepth = depth / farPlane;
    // gl_FragDepth = distance / farPlane;
}