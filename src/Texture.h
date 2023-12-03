#pragma once

#include <glad/glad.h>

struct Texture
{
	GLuint id;

	// Used as default textures in order to treat materials consistently, whether they have actual textures or not
	static Texture White1x1TextureRGBA();
	static Texture Max1x1TextureRed();
	static Texture DepthCubemap1x1();
};