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
