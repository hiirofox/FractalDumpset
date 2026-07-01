#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include "External/glad/include/glad/glad.h"
//#include "External/glfw-3.4/include/GLFW/glfw3.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

class ObjComponent {
public:
	void Init(std::string path) {
		LoadOBJ(path);
		SetupGPU();
	}

	~ObjComponent() {
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
	}

	// ---------------- Transform API ----------------
	ObjComponent& Reset()
	{
		position = glm::vec3{ 0.0f };
		rotation = glm::vec3{ 0.0f };
		scale = glm::vec3{ 1.0f };
		return *this;
	}
	ObjComponent& Move(float x, float y, float z) {
		position += glm::vec3(x, y, z);
		return *this;
	}

	ObjComponent& Rotate(float x, float y, float z) {
		rotation += glm::vec3(x, y, z);
		return *this;
	}

	ObjComponent& Scale(float s) {
		scale *= s;
		return *this;
	}

	// ---------------- Render ----------------
	void Draw(GLuint shaderProgram) {
		glm::mat4 model = GetModelMatrix();

		glUseProgram(shaderProgram);
		glUniformMatrix4fv(
			glGetUniformLocation(shaderProgram, "model"),
			1,
			GL_FALSE,
			glm::value_ptr(model)
		);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES,
			(GLsizei)indices.size(),
			GL_UNSIGNED_INT,
			0);
	}


	// ---------------- Model Matrix ----------------
	glm::mat4 GetModelMatrix() const {
		glm::mat4 m = glm::mat4(1.0f);

		m = glm::translate(m, position);

		m = glm::rotate(m, rotation.x, glm::vec3(1, 0, 0));
		m = glm::rotate(m, rotation.y, glm::vec3(0, 1, 0));
		m = glm::rotate(m, rotation.z, glm::vec3(0, 0, 1));

		m = glm::scale(m, scale);

		return m;
	}
private:
	// ---------------- Vertex ----------------
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
	};

	// ---------------- OBJ data ----------------
	std::vector<glm::vec3> temp_positions;
	std::vector<glm::vec3> temp_normals;

	struct Face {
		std::vector<int> v;   // 支持 N-gon
		std::vector<int> n;
	};

	std::vector<Face> faces;

	// ---------------- GPU data ----------------
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	GLuint VAO = 0, VBO = 0, EBO = 0;

	// ---------------- Transform ----------------
	glm::vec3 position{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f };

private:

	// ---------------- Load OBJ ----------------
	void LoadOBJ(const std::string& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			std::cerr << "Failed to open OBJ: " << path << std::endl;
			return;
		}

		std::string line;

		while (std::getline(file, line)) {
			std::istringstream ss(line);
			std::string type;
			ss >> type;

			if (type == "v") {
				glm::vec3 v;
				ss >> v.x >> v.y >> v.z;
				temp_positions.push_back(v);
			}
			else if (type == "vn") {
				glm::vec3 n;
				ss >> n.x >> n.y >> n.z;
				temp_normals.push_back(n);
			}
			else if (type == "f") {

				Face f;
				std::string token;

				while (ss >> token) {
					int v = 0, n = 0;

					// Blender: v//n
					int result = sscanf(token.c_str(), "%d//%d", &v, &n);

					f.v.push_back(v - 1);

					if (result == 2)
						f.n.push_back(n - 1);
					else
						f.n.push_back(-1); // ⭐关键：允许没有normal
				}

				faces.push_back(f);
			}
		}

		BuildMesh();
	}

	// ---------------- Build Mesh (FIXED) ----------------
	void BuildMesh()
	{
		vertices.clear();
		indices.clear();

		for (const auto& f : faces)
		{
			glm::vec3 p0 = temp_positions[f.v[0]];
			glm::vec3 p1 = temp_positions[f.v[1]];
			glm::vec3 p2 = temp_positions[f.v[2]];

			glm::vec3 normal = glm::normalize(
				glm::cross(p1 - p0, p2 - p0)
			);

			for (int i = 0; i < 3; i++)
			{
				Vertex vert;
				vert.position = temp_positions[f.v[i]];
				vert.normal = normal;

				vertices.push_back(vert);
				indices.push_back((unsigned int)indices.size());
			}
		}
	}

	// ---------------- Add Vertex ----------------
	void AddVertex(const Face& f, int i) {
		Vertex vert;

		int vi = f.v[i];
		int ni = (i < f.n.size()) ? f.n[i] : -1;

		// ---- position 安全检查 ----
		if (vi < 0 || vi >= temp_positions.size()) {
			std::cerr << "Vertex index out of range: " << vi << std::endl;
			return;
		}

		vert.position = temp_positions[vi];

		// ---- normal 安全检查 ----
		if (ni >= 0 && ni < temp_normals.size()) {
			vert.normal = temp_normals[ni];
		}
		else {
			vert.normal = glm::vec3(0, 1, 0); // fallback normal
		}

		vertices.push_back(vert);
		indices.push_back((unsigned int)vertices.size() - 1);
	}

	// ---------------- Upload to GPU ----------------
	void SetupGPU() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER,
			vertices.size() * sizeof(Vertex),
			vertices.data(),
			GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(unsigned int),
			indices.data(),
			GL_STATIC_DRAW);

		// position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			sizeof(Vertex),
			(void*)0);
		glEnableVertexAttribArray(0);

		// normal
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
			sizeof(Vertex),
			(void*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

};