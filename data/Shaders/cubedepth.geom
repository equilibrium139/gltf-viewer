layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 lightProjectionMatrices[6];

out vec4 surfacePosWS;

void main()
{
    for (int face = 0; face < 6; face++)
    {
        gl_Layer = face;
        for (int i = 0; i < 3; i++)
        {
            surfacePosWS = gl_in[i].gl_Position;
            gl_Position = lightProjectionMatrices[face] * surfacePosWS;
            EmitVertex();
        }
        EndPrimitive();
    }
}