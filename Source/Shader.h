#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include "External/glad/include/glad/glad.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"

class Shader {
public:
    GLuint ID;

    void Init(const char* vertexPath, const char* fragmentPath) {
        std::string vCode = LoadFile(vertexPath);
        std::string fCode = LoadFile(fragmentPath);

        GLuint vertex = Compile(GL_VERTEX_SHADER, vCode.c_str());
        GLuint fragment = Compile(GL_FRAGMENT_SHADER, fCode.c_str());

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        CheckLink(ID);

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void Use() {
        glUseProgram(ID);
    }

    void SetMat4(const std::string& name, const glm::mat4& mat) {
        glUniformMatrix4fv(
            glGetUniformLocation(ID, name.c_str()),
            1,
            GL_FALSE,
            &mat[0][0]
        );
    }

private:

    std::string LoadFile(const char* path) {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    GLuint Compile(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        CheckCompile(shader, type);

        return shader;
    }

    void CheckCompile(GLuint shader, GLenum type) {
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            GLchar infoLog[1024];
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);

            std::cout << "SHADER COMPILE ERROR\n" << infoLog << std::endl;
        }
    }

    void CheckLink(GLuint program) {
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success) {
            GLchar infoLog[1024];
            glGetProgramInfoLog(program, 1024, NULL, infoLog);

            std::cout << "PROGRAM LINK ERROR\n" << infoLog << std::endl;
        }
    }
};