layout(location = 0) in vec3 aBasePos;

uniform mat4 modelView;
uniform mat4 projection;

void main()
{
    gl_Position = projection * modelView * vec4(aBasePos, 1.0);
}