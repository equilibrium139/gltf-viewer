uniform float farPlane;
uniform vec3 lightPosWS;

in vec4 surfacePosWS;

void main()
{
    float distance = length(surfacePosWS.xyz - lightPosWS);
    gl_FragDepth = distance / farPlane;
}