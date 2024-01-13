layout(location=0) in vec3 aBasePos;
layout(location=1) in vec3 aCubemapDir;

out vec3 dir;

void main()
{
    dir = aCubemapDir;
    gl_Position = vec4(aBasePos, 1.0);
}