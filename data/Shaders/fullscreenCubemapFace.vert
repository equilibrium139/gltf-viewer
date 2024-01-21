// Cubemap faces have interesting UV coordinate spaces: https://www.khronos.org/opengl/wiki/Cubemap_Texture
// So this shader allows for dealing with that
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 texCoords;

uniform bool flipUV;

void main()
{
    texCoords = aTexCoords;
    if (flipUV)
    {
        texCoords.x = 1.0 - texCoords.x;
        texCoords.y = 1.0 - texCoords.y;
    }

    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}  