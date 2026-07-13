#include "Shader.h"

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
	this->vertexPath = vertexPath;
	this->fragmentPath = fragmentPath;

	std::ifstream vertexFile;
	std::ifstream fragmentFile;
	std::string vertexCode;
	std::string fragmentCode;

	vertexFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fragmentFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try {
		vertexFile.open(vertexPath);
		fragmentFile.open(fragmentPath);
		std::stringstream vertexStream, fragmentStream;
		vertexStream << vertexFile.rdbuf();
		fragmentStream << fragmentFile.rdbuf();
		vertexCode = vertexStream.str();
		fragmentCode = fragmentStream.str();
	}
	catch(std::ifstream::failure e){
		std::cout << "FILE NOT SUCCESSFULLY READ" << std::endl;
	}

	const char* vertexSrc = vertexCode.c_str();
	const char* fragmentSrc = fragmentCode.c_str();

	unsigned int vertexShader;
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSrc, NULL);
	glCompileShader(vertexShader);

	unsigned int fragmentShader;
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSrc, NULL);
	glCompileShader(fragmentShader);

	this->ID = glCreateProgram();
	glAttachShader(this->ID, vertexShader);
	glAttachShader(this->ID, fragmentShader);
	glLinkProgram(this->ID);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::use() {
	glUseProgram(this->ID);
}

void Shader::setBool(const std::string& name, bool value) const {
	glUniform1i(glGetUniformLocation(this->ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const {
	glUniform1i(glGetUniformLocation(this->ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const {
	glUniform1f(glGetUniformLocation(this->ID, name.c_str()), value);
}

void Shader::setSampler2D(const std::string& name, int value) const {
	glUniform1i(glGetUniformLocation(this->ID, name.c_str()), value);
}

void Shader::setMat4D(const std::string& name, glm::mat4 value) const {
	glUniformMatrix4fv(glGetUniformLocation(this->ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec2(const std::string& name, glm::vec2 value) const {
	glUniform2f(glGetUniformLocation(this->ID, name.c_str()), value.x, value.y);
}

void Shader::setVec4D(const std::string& name, glm::vec4 value) const {
	glad_glUniform4f(glGetUniformLocation(this->ID, name.c_str()), value.x, value.y, value.z, value.a);
}