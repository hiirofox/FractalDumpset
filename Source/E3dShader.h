#pragma once

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

class ShaderCompiler
{
private:
public:
	GLuint CompileShaderGLSL(const char* vertex, const char* fragment)
	{
		GLint success = 0;
		GLchar infoLog[1024];

		// --------------------
		// Compile vertex shader
		// --------------------
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertex, nullptr);
		glCompileShader(vertexShader);

		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(vertexShader, 1024, nullptr, infoLog);
			std::cout << "Vertex Shader Compile Error:\n" << infoLog << std::endl;

			glDeleteShader(vertexShader);
			return 0;
		}

		// --------------------
		// Compile fragment shader
		// --------------------
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragment, nullptr);
		glCompileShader(fragmentShader);

		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(fragmentShader, 1024, nullptr, infoLog);
			std::cout << "Fragment Shader Compile Error:\n" << infoLog << std::endl;

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			return 0;
		}

		// --------------------
		// Link shader program
		// --------------------
		GLuint program = glCreateProgram();

		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(program, 1024, nullptr, infoLog);
			std::cout << "Shader Program Link Error:\n" << infoLog << std::endl;

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			glDeleteProgram(program);
			return 0;
		}

		// Program 链接完成后，shader object 可以删除
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return program;
	}
};