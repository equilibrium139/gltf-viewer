#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

//#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/type_ptr.hpp>

struct UniformBlockBinding
{
	std::string uniform_block_name;
	GLuint uniform_block_binding;
};

class Shader
{
public:
	unsigned int id;
	Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr, const std::vector<UniformBlockBinding>& ub_bindings = {},
		const std::vector<std::string> defines = {});


	void use();

	void SetBool(const char* name, bool value);
	void SetInt(const char* name, int value);
	void SetUint(const char* name, std::uint32_t value);
	void SetFloat(const char* name, float value);
	void SetMat4(const char* name, const float* value);
	void SetMat4(const char* name, const float* value, int count);
	void SetMat3(const char* name, const float* value);
	void SetVec3(const char* name, float x, float y, float z);
	void SetVec3(const char* name, const glm::vec3& vec);
	void SetVec4(const char* name, float x, float y, float z, float w);
	void SetVec4(const char* name, const glm::vec4& vec);
	void SetVec3Array(const char* name, float* values, unsigned int count);
private:
	std::string get_file_contents(const char* path);
	std::unordered_map<std::string, int> cachedUniformLocations;

	int GetUniformLocation(const std::string& name)
	{
		auto iter = cachedUniformLocations.find(name);
		if (iter != cachedUniformLocations.end())
		{
			return iter->second;
		}
		int location = glGetUniformLocation(id, name.c_str());
		cachedUniformLocations[name] = location;
		return location;
	}
};

#endif // !SHADER_H