#include "Shader.h"

Shader::Shader(const char * vertexPath, const char * fragmentPath, const char * geometryPath, const std::vector<UniformBlockBinding>& ub_bindings, const std::vector<std::string> defines)
{
	static const std::string version = "#version 330 core\n";

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	auto vertexSource = get_file_contents(vertexPath);
	auto fragmentSource = get_file_contents(fragmentPath);

	auto vShaderCode = vertexSource.c_str();
	auto fShaderCode = fragmentSource.c_str();

	std::string definesString;
	for (const std::string& define : defines)
	{
		definesString += "#define " + define + "\n";
	}

	const char* vShaderSources[3] = { version.c_str(), definesString.c_str(), vShaderCode};
	const char* fShaderSources[3] = { version.c_str(), definesString.c_str(), fShaderCode};
	
	glShaderSource(vertexShader, 3, vShaderSources, NULL);
	glShaderSource(fragmentShader, 3, fShaderSources, NULL);

	glCompileShader(vertexShader);

	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << '\n';
		// TODO find a solution for this. It's affecting the next Shader object created
		// when this one fails
	}

	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << '\n';
	}

	id = glCreateProgram();
	glAttachShader(id, vertexShader);
	glAttachShader(id, fragmentShader);

	if (geometryPath != nullptr) {
		unsigned int geomShader = glCreateShader(GL_GEOMETRY_SHADER);
		auto geomSource = get_file_contents(geometryPath);
		auto code = geomSource.c_str();
		glShaderSource(geomShader, 1, &code, NULL);
		glCompileShader(geomShader);
		glGetShaderiv(geomShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(geomShader, sizeof(infoLog), NULL, infoLog);
			std::cout << "ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n" << infoLog << '\n';
		}

		glAttachShader(id, geomShader);
	}

	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(id, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << '\n';
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	use();
	for (const auto& binding : ub_bindings)
	{
		auto block_index = glGetUniformBlockIndex(id, binding.uniform_block_name.c_str());
		glUniformBlockBinding(id, block_index, binding.uniform_block_binding);
	}
}

void Shader::use()
{
	glUseProgram(id);
}

void Shader::SetBool(const char * name, bool value) const
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform1i(uniformLocation, value);
}

void Shader::SetInt(const char * name, int value) const
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform1i(uniformLocation, value);
}

void Shader::SetUint(const char* name, std::uint32_t value) const
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform1ui(uniformLocation, value);
}

void Shader::SetFloat(const char * name, float value) const
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform1f(uniformLocation, value);
}

void Shader::SetMat4(const char * name, const float * value)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, value);
}

void Shader::SetMat4(const char* name, const float* value, int count)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniformMatrix4fv(uniformLocation, count, GL_FALSE, value);
}

void Shader::SetMat3(const char* name, const float* value)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniformMatrix3fv(uniformLocation, 1, GL_FALSE, value);
}

void Shader::SetVec3(const char * name, float x, float y, float z)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform3f(uniformLocation, x, y, z);
}

void Shader::SetVec3(const char* name, const glm::vec3& vec)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform3f(uniformLocation, vec.x, vec.y, vec.z);
}

void Shader::SetVec4(const char* name, float x, float y, float z, float w)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform4f(uniformLocation, x, y, z, w);
}

void Shader::SetVec4(const char* name, const glm::vec4& vec)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform4f(uniformLocation, vec.x, vec.y, vec.z, vec.w);
}

void Shader::SetVec3Array(const char* name, float* values, unsigned int count)
{
	int uniformLocation = glGetUniformLocation(id, name);
	glUniform3fv(uniformLocation, count, values);
}

std::string Shader::get_file_contents(const char * path)
{
	std::ifstream in(path);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return contents;
	}
	else
	{
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: '" << path << "'\n";
		return {};
	}
}
