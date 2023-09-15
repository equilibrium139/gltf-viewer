out vec4 color;

uniform sampler2D sceneColorsTexture;
uniform sampler2D highlightTexture;

in vec2 texCoords;

void main()
{
    color = vec4(1.0, 0.0, 0.0, 1.0);
    const float offset = 1.0 / 1000.0;  
    vec2 offsets[9] = vec2[](
        vec2(-offset,  offset), // top-left
        vec2( 0.0f,    offset), // top-center
        vec2( offset,  offset), // top-right
        vec2(-offset,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( offset,  0.0f),   // center-right
        vec2(-offset, -offset), // bottom-left
        vec2( 0.0f,   -offset), // bottom-center
        vec2( offset, -offset)  // bottom-right    
    );
    float r = texture(highlightTexture, texCoords).r;
    bool highlight = false;
    if (r == 0.0)
    {
        for (int i = 0; i < 9; i++)
        {
            float n = texture(highlightTexture, texCoords + offsets[i]).r;
            if (n != 0.0)
            {
                highlight = true;
                break;
            }
        }
    }
    if (!highlight)
    {
        color = vec4(texture(sceneColorsTexture, texCoords).rgb, 1.0);
    }
    else 
    {
        color = vec4(1.0, 0.0, 0.0, 1.0);
    }
}