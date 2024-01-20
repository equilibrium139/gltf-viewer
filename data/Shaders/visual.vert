layout(location=0) in vec3 aBasePos;

uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(aBasePos, 1.0);
}