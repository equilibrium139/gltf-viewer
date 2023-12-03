#include "Texture.h"

Texture Texture::White1x1TextureRGBA()
{
	static Texture texture;
	static bool firstTime = true;

	if (firstTime)
	{
		glGenTextures(1, &texture.id);
		glBindTexture(GL_TEXTURE_2D, texture.id);
		GLubyte data[4] = { 255, 255, 255, 255 };
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		firstTime = false;
	}

	return texture;
}

Texture Texture::Max1x1TextureRed()
{
	static Texture texture;
	static bool firstTime = true;

	if (firstTime)
	{
		glGenTextures(1, &texture.id);
		glBindTexture(GL_TEXTURE_2D, texture.id);
		GLubyte data[1] = { 255 };
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, data);
		firstTime = false;
	}

	return texture;
}

Texture Texture::DepthCubemap1x1()
{
	static Texture texture;
	static bool firstTime = true;

	if (firstTime)
	{
		glGenTextures(1, &texture.id);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture.id);
		GLubyte data[1] = { 255 };
		for (int i = 0; i < 6; i++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, data);
		}
		firstTime = false;
	}

	return texture;
}