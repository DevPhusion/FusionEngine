#ifndef SHADER_H
#define SHADER_H
#pragma once

#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Editor/Windows/Console.h"

class Shader
{
public:
	Shader(const char* vertexPath, const char* fragmentPath);
	Shader() = default;

	std::string vertexPath;
	std::string fragmentPath;

	void use();
	void setBool(const std::string& name, bool value) const;
	void setInt(const std::string& name, int value) const;
	void setFloat(const std::string& name, float value) const;
	void setVec2D(const std::string& name, glm::vec2 value) const;
	void setSampler2D(const std::string& name, int value) const;
	void setMat4D(const std::string& name, glm::mat4 value) const;
	void setVec4D(const std::string& name, glm::vec4 value) const;
private:
	static std::unordered_map<std::string, std::string>& SourceCache();
	static const std::string& LoadOrGetCachedSource(const std::string& path);
	unsigned int ID;
};

#endif
