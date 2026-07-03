#pragma once

#include "../Resources_Resources.rsh"
#include <windows.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Test3DObj.h"
#include "TestTerrain.h"

class ViewController :public Enola2::EventListener
{
private://camera setting
	glm::vec3 cameraPos = glm::vec3(-10.0f, 20.0f, 0.0f);
	glm::vec3 cameraTarget = glm::vec3(0.0f, 20.0f, 0.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 view = glm::lookAt(
		cameraPos,  // 相机在“上方”
		cameraTarget,  // 看向原点
		cameraUp  // “上方向”
	);

	bool middleMouseDown = false;
	bool shiftDown = false;

	float lastMouseX = 0.0f;
	float lastMouseY = 0.0f;

	float orbitSpeed = 0.005f;
	float panSpeed = 0.0015f;

	float wheelZoomFactor = 88.0f;      // 每滚一格缩放倍率，小于 1 表示靠近
	float minZoomDistance = 1.0f;       // 离 target 最近距离
	float maxZoomDistance = 2000.0f;    // 离 target 最远距离

	glm::mat4 UpdateCameraView()
	{
		view = glm::lookAt(
			cameraPos,
			cameraTarget,
			cameraUp
		);
		return view;
	}
protected:

	void OnMouse(const Enola2::MouseEvent& e) override
	{
		// =========================
		// 鼠标滚轮：Blender 风格前进 / 后退
		// =========================
		if (e.state == Enola2::MouseState::Wheel)
		{
			glm::vec3 offset = cameraPos - cameraTarget;

			float distance = glm::length(offset);
			if (distance <= 0.0001f)
				return;

			glm::vec3 dir = glm::normalize(offset);

			// Win32 WM_MOUSEWHEEL 通常一格是 120
			float steps = (float)e.wheel / 120.0f;

			// 如果你的 e.wheel 只是 +1 / -1，也能兼容
			if (fabs(steps) < 0.001f)
				steps = e.wheel > 0 ? 1.0f : -1.0f;

			// wheel > 0：靠近 target
			// wheel < 0：远离 target
			float factor = pow(wheelZoomFactor, -steps);

			float newDistance = distance * factor;

			if (newDistance < minZoomDistance)
				newDistance = minZoomDistance;

			if (newDistance > maxZoomDistance)
				newDistance = maxZoomDistance;

			cameraPos = cameraTarget + dir * newDistance;

			UpdateCameraView();
			return;
		}

		// 你说 WheelDown / WheelUp 表示中键按下 / 松开
		if (e.state == Enola2::MouseState::WheelDown)
		{
			middleMouseDown = true;
			lastMouseX = (float)e.x;
			lastMouseY = (float)e.y;
			return;
		}

		if (e.state == Enola2::MouseState::WheelUp)
		{
			middleMouseDown = false;
			return;
		}

		if (e.state != Enola2::MouseState::Move)
			return;

		if (!middleMouseDown)
			return;

		float x = (float)e.x;
		float y = (float)e.y;

		float dx = x - lastMouseX;
		float dy = y - lastMouseY;

		lastMouseX = x;
		lastMouseY = y;

		// =========================
		// Shift + 鼠标中键拖动：平移视角
		// =========================
		if (shiftDown)
		{
			glm::vec3 forward = glm::normalize(cameraTarget - cameraPos);
			glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp));
			glm::vec3 up = glm::normalize(glm::cross(right, forward));

			float distance = glm::length(cameraTarget - cameraPos);
			float scale = distance * panSpeed;

			glm::vec3 move =
				-right * dx * scale +
				up * dy * scale;

			cameraPos += move;
			cameraTarget += move;

			UpdateCameraView();
			return;
		}

		// =========================
		// 鼠标中键拖动：围绕 target 旋转视角
		// Blender 风格 Orbit
		// =========================
		glm::vec3 offset = cameraPos - cameraTarget;

		float yaw = -dx * orbitSpeed;
		float pitch = -dy * orbitSpeed;

		// 水平旋转：绕世界 up 轴
		glm::mat4 yawMat = glm::rotate(
			glm::mat4(1.0f),
			yaw,
			cameraUp
		);

		offset = glm::vec3(yawMat * glm::vec4(offset, 0.0f));

		// 垂直旋转：绕当前视角 right 轴
		glm::vec3 forward = glm::normalize(cameraTarget - (cameraTarget + offset));
		glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp));

		glm::mat4 pitchMat = glm::rotate(
			glm::mat4(1.0f),
			pitch,
			right
		);

		glm::vec3 newOffset = glm::vec3(
			pitchMat * glm::vec4(offset, 0.0f)
		);

		// 防止翻到头顶后相机上下颠倒
		float vertical = glm::dot(
			glm::normalize(newOffset),
			cameraUp
		);

		if (fabs(vertical) < 0.98f)
		{
			offset = newOffset;
		}

		cameraPos = cameraTarget + offset;

		UpdateCameraView();
	}

	void OnKey(const Enola2::KeyEvent& e) override
	{
		if (e.key == VK_LSHIFT || e.key == VK_RSHIFT || e.key == VK_SHIFT)
		{
			if (e.state == Enola2::KeyState::Down)
				shiftDown = true;
			else if (e.state == Enola2::KeyState::Up)
				shiftDown = false;
		}
	}
public://处理camera操作逻辑
	ViewController()
	{
		Enola2::EventListener::Register(this);
	}
	void SetCamera(glm::vec3 pos, glm::vec3 target, glm::vec3 up)
	{
		cameraPos = pos;
		cameraTarget = target;
		cameraUp = up;
		UpdateCameraView();
	}
	glm::mat4 GetNowView()
	{
		//UpdateCameraView();
		return view;
	}
	glm::vec3 GetNowCameraPos()
	{
		//UpdateCameraView();
		return cameraPos;
	}
};


class ImageLoader
{
public:
	GLuint LoadTexture2D(const std::string& path, bool flipY = true)
	{
		int width = 0;
		int height = 0;
		int channels = 0;

		stbi_set_flip_vertically_on_load(flipY);

		unsigned char* data = stbi_load(
			path.c_str(),
			&width,
			&height,
			&channels,
			0
		);

		if (!data)
		{
			printf("Image load failed: %s\n", path.c_str());
			return 0;
		}

		GLenum format = GL_RGB;

		if (channels == 1)
			format = GL_RED;
		else if (channels == 3)
			format = GL_RGB;
		else if (channels == 4)
			format = GL_RGBA;

		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			format,
			width,
			height,
			0,
			format,
			GL_UNSIGNED_BYTE,
			data
		);

		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindTexture(GL_TEXTURE_2D, 0);

		stbi_image_free(data);

		return texture;
	}
};


class ObjLoader
{
private:
	struct MtlGroup
	{
		std::string newmtl;//newmtl xxx
		float ns;//Ns 
		glm::vec3 ka;//Ka
		glm::vec3 kd;//Kd
		glm::vec3 ks;//Ks
		glm::vec3 ke;//Ke
		float ni;//Ni
		float d;//d
		int illum;//illum

		std::string mapKd;//贴图文件
		GLuint diffuseTexture = 0;
		bool hasDiffuseTexture = false;
	};
	std::unordered_map<std::string, MtlGroup> bucketMtl;
	std::vector<glm::vec3> bucketV;
	std::vector<glm::vec3> bucketVn;
	std::vector<glm::vec2> bucketVt;
	struct FaceGroup
	{
		std::string usemtl;//usemtl
		struct FaceIndex
		{
			//f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
			int v1, v2, v3;
			int vt1, vt2, vt3;
			int vn1, vn2, vn3;
		};
		std::vector<FaceIndex> faces;
	};
	std::vector<FaceGroup> bucketFace;


private://VAO/VBO
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
	};
	struct RenderGroup
	{
		std::string usemtl;
		unsigned int firstVertex;
		unsigned int vertexCount;
	};
	std::vector<Vertex> vertices;
	std::vector<RenderGroup> renderGroups;

public:
	void LoadObj(std::string objPath, std::string mtlPath)
	{
		bucketMtl.clear();
		bucketV.clear();
		bucketVn.clear();
		bucketVt.clear();
		bucketFace.clear();
		bucketV.push_back({ 0,0,0 });//这样index从1开始
		bucketVn.push_back({ 0,0,0 });
		bucketVt.push_back({ 0,0 });

		auto GetDirectory = [](const std::string& path) -> std::string
			{
				size_t p1 = path.find_last_of('/');
				size_t p2 = path.find_last_of('\\');

				size_t p = std::string::npos;

				if (p1 != std::string::npos && p2 != std::string::npos)
				{
					if (p1 > p2)
						p = p1;
					else
						p = p2;
				}
				else if (p1 != std::string::npos)
					p = p1;
				else
					p = p2;

				if (p == std::string::npos)
					return "";

				return path.substr(0, p + 1);
			};

		std::string mtlDir = GetDirectory(mtlPath);
		ImageLoader imageLoader;

		// --------------------
		// Load MTL
		// --------------------
		{
			std::ifstream file(mtlPath);
			if (file.is_open())
			{
				std::string line;
				MtlGroup* currentMtl = nullptr;

				while (std::getline(file, line))
				{
					if (line.empty())
						continue;

					std::stringstream ss(line);
					std::string tag;
					ss >> tag;

					if (tag.empty())
						continue;

					// 注释行
					if (tag[0] == '#')
						continue;

					if (tag == "newmtl")
					{
						MtlGroup mtl{};

						mtl.ns = 0.0f;
						mtl.ka = glm::vec3(1.0f);
						mtl.kd = glm::vec3(1.0f);
						mtl.ks = glm::vec3(0.0f);
						mtl.ke = glm::vec3(0.0f);
						mtl.ni = 1.0f;
						mtl.d = 1.0f;
						mtl.illum = 2;
						mtl.mapKd = "";
						mtl.diffuseTexture = 0;
						mtl.hasDiffuseTexture = false;

						ss >> mtl.newmtl;

						bucketMtl[mtl.newmtl] = mtl;
						currentMtl = &bucketMtl[mtl.newmtl];
					}
					else if (currentMtl != nullptr)
					{
						if (tag == "Ns")
						{
							ss >> currentMtl->ns;
						}
						else if (tag == "Ka")
						{
							ss >> currentMtl->ka.x >> currentMtl->ka.y >> currentMtl->ka.z;
						}
						else if (tag == "Kd")
						{
							ss >> currentMtl->kd.x >> currentMtl->kd.y >> currentMtl->kd.z;
						}
						else if (tag == "Ks")
						{
							ss >> currentMtl->ks.x >> currentMtl->ks.y >> currentMtl->ks.z;
						}
						else if (tag == "Ke")
						{
							ss >> currentMtl->ke.x >> currentMtl->ke.y >> currentMtl->ke.z;
						}
						else if (tag == "Ni")
						{
							ss >> currentMtl->ni;
						}
						else if (tag == "d")
						{
							ss >> currentMtl->d;
						}
						else if (tag == "illum")
						{
							ss >> currentMtl->illum;
						}
						else if (tag == "map_Kd")//顺便读取图像数据
						{
							ss >> currentMtl->mapKd;

							std::string texturePath = mtlDir + currentMtl->mapKd;
							printf("mtl %s -> %s\n", currentMtl->newmtl.c_str(), texturePath.c_str());
							currentMtl->diffuseTexture = imageLoader.LoadTexture2D(texturePath);

							if (currentMtl->diffuseTexture != 0)
							{
								currentMtl->hasDiffuseTexture = true;
							}
							else
							{
								currentMtl->hasDiffuseTexture = false;
								printf("Failed to load map_Kd texture: %s\n", texturePath.c_str());
							}
						}
					}
				}
			}
		}

		// --------------------
		// Load OBJ
		// --------------------
		{
			std::ifstream file(objPath);
			if (!file.is_open())
				return;

			std::string line;
			FaceGroup* currentFaceGroup = nullptr;

			while (std::getline(file, line))
			{
				if (line.empty())
					continue;

				std::stringstream ss(line);
				std::string tag;
				ss >> tag;

				if (tag == "v")
				{
					glm::vec3 v{};
					ss >> v.x >> v.y >> v.z;
					bucketV.push_back(v);
				}
				else if (tag == "vn")
				{
					glm::vec3 vn{};
					ss >> vn.x >> vn.y >> vn.z;
					bucketVn.push_back(vn);
				}
				else if (tag == "vt")
				{
					glm::vec2 vt{};
					ss >> vt.x >> vt.y;
					bucketVt.push_back(vt);
				}
				else if (tag == "usemtl")
				{
					std::string usemtl;
					ss >> usemtl;

					bucketFace.push_back(FaceGroup{});
					bucketFace.back().usemtl = usemtl;
					currentFaceGroup = &bucketFace.back();
				}
				else if (tag == "f")
				{
					if (currentFaceGroup == nullptr)
					{
						bucketFace.push_back(FaceGroup{});
						currentFaceGroup = &bucketFace.back();
					}

					FaceGroup::FaceIndex face{};
					char slash;

					ss >> face.v1 >> slash >> face.vt1 >> slash >> face.vn1;
					ss >> face.v2 >> slash >> face.vt2 >> slash >> face.vn2;
					ss >> face.v3 >> slash >> face.vt3 >> slash >> face.vn3;

					currentFaceGroup->faces.push_back(face);
				}
			}
		}
	}

	GLuint vao = 0, vbo = 0;
	void CreateVAOVBO()
	{
		if (vao != 0)
			glDeleteVertexArrays(1, &vao);
		if (vbo != 0)
			glDeleteBuffers(1, &vbo);
		vertices.clear();
		renderGroups.clear();

		for (const auto& group : bucketFace)
		{
			RenderGroup renderGroup{};
			renderGroup.usemtl = group.usemtl;
			renderGroup.firstVertex = static_cast<unsigned int>(vertices.size());

			for (const auto& face : group.faces)
			{
				Vertex v1{};
				v1.pos = bucketV[face.v1];
				v1.normal = bucketVn[face.vn1];
				v1.uv = glm::vec2(bucketVt[face.vt1].x, bucketVt[face.vt1].y);

				Vertex v2{};
				v2.pos = bucketV[face.v2];
				v2.normal = bucketVn[face.vn2];
				v2.uv = glm::vec2(bucketVt[face.vt2].x, bucketVt[face.vt2].y);

				Vertex v3{};
				v3.pos = bucketV[face.v3];
				v3.normal = bucketVn[face.vn3];
				v3.uv = glm::vec2(bucketVt[face.vt3].x, bucketVt[face.vt3].y);

				vertices.push_back(v1);
				vertices.push_back(v2);
				vertices.push_back(v3);
			}

			renderGroup.vertexCount =
				static_cast<unsigned int>(vertices.size()) - renderGroup.firstVertex;

			renderGroups.push_back(renderGroup);
		}

		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(vao);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(
			GL_ARRAY_BUFFER,
			vertices.size() * sizeof(Vertex),
			vertices.data(),
			GL_STATIC_DRAW
		);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			(void*)offsetof(Vertex, pos)
		);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			(void*)offsetof(Vertex, normal)
		);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(
			2,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(Vertex),
			(void*)offsetof(Vertex, uv)
		);

		glBindVertexArray(0);
	}

	void Draw(GLuint shaderProgram,
		const glm::mat4& model,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPos)
	{
		glUseProgram(shaderProgram);

		//test
		/*glUniform3f(
			glGetUniformLocation(shaderProgram, "uLightDir"),
			-0.3f, -1.0f, -0.5f
		);*/

		glUniformMatrix4fv(
			glGetUniformLocation(shaderProgram, "uModel"),
			1,
			GL_FALSE,
			glm::value_ptr(model)
		);

		glUniformMatrix4fv(
			glGetUniformLocation(shaderProgram, "uView"),
			1,
			GL_FALSE,
			glm::value_ptr(view)
		);

		glUniformMatrix4fv(
			glGetUniformLocation(shaderProgram, "uProjection"),
			1,
			GL_FALSE,
			glm::value_ptr(projection)
		);

		glUniform3fv(
			glGetUniformLocation(shaderProgram, "uViewPos"),
			1,
			glm::value_ptr(cameraPos)
		);

		glBindVertexArray(vao);

		for (const auto& group : renderGroups)
		{
			const MtlGroup& mtl = bucketMtl[group.usemtl];

			glUniform3fv(
				glGetUniformLocation(shaderProgram, "uKd"),
				1,
				glm::value_ptr(mtl.kd)
			);

			glUniform1f(
				glGetUniformLocation(shaderProgram, "uD"),
				mtl.d
			);

			if (mtl.hasDiffuseTexture)
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, mtl.diffuseTexture);

				glUniform1i(
					glGetUniformLocation(shaderProgram, "uDiffuseMap"),
					0
				);

				glUniform1i(
					glGetUniformLocation(shaderProgram, "uHasDiffuseTexture"),
					1
				);
			}
			else
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, 0);

				glUniform1i(
					glGetUniformLocation(shaderProgram, "uHasDiffuseTexture"),
					0
				);
			}

			glDrawArrays(
				GL_TRIANGLES,
				group.firstVertex,
				group.vertexCount
			);
		}

		glBindVertexArray(0);
	}
};


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


class GridObj
{
private:

	GLuint gridVAO = 0;
	GLuint gridVBO = 0;
	int gridLineCount = 0;

public:
	void InitGrid(float halfSize = 10.0f, int lines = 200)
	{
		std::vector<glm::vec3> vertices;
		std::vector<unsigned int> indices;

		float step = (halfSize * 2.0f) / lines;

		// 1. 生成“交叉点顶点”
		for (int z = 0; z <= lines; z++)
		{
			for (int x = 0; x <= lines; x++)
			{
				float px = -halfSize + x * step;
				float pz = -halfSize + z * step;

				vertices.push_back(glm::vec3(px, 0.0f, pz));
			}
		}

		// 2. 生成横向线索引
		for (int z = 0; z <= lines; z++)
		{
			for (int x = 0; x < lines; x++)
			{
				int i0 = z * (lines + 1) + x;
				int i1 = i0 + 1;

				indices.push_back(i0);
				indices.push_back(i1);
			}
		}

		// 3. 生成纵向线索引
		for (int z = 0; z < lines; z++)
		{
			for (int x = 0; x <= lines; x++)
			{
				int i0 = z * (lines + 1) + x;
				int i1 = i0 + (lines + 1);

				indices.push_back(i0);
				indices.push_back(i1);
			}
		}

		gridLineCount = (int)indices.size();

		GLuint EBO;

		glGenVertexArrays(1, &gridVAO);
		glGenBuffers(1, &gridVBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(gridVAO);

		// vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
		glBufferData(GL_ARRAY_BUFFER,
			vertices.size() * sizeof(glm::vec3),
			vertices.data(),
			GL_STATIC_DRAW);

		// index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(unsigned int),
			indices.data(),
			GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

		glBindVertexArray(0);
	}
	void Draw(GLuint shaderProgram,
		const glm::mat4& model,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPos)
	{

		glUseProgram(shaderProgram);

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
			1, GL_FALSE, glm::value_ptr(model));

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
			1, GL_FALSE, glm::value_ptr(view));

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
			1, GL_FALSE, glm::value_ptr(projection));

		glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"),
			1, glm::value_ptr(cameraPos));

		glBindVertexArray(gridVAO);
		glDrawElements(GL_LINES, gridLineCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

};

class Model :public Enola2::Component
{
private://shader
	const char* gridVertexSL = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 WorldPos;

void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    WorldPos = world.xyz;

    gl_Position = projection * view * world;
}
)";
	const char* gridFragmentSL = R"(
#version 330 core

in vec3 WorldPos;

uniform vec3 cameraPos;

out vec4 FragColor;

void main()
{
    vec3 gridColor = vec3(0.32, 0.32, 0.32);
    vec3 xAxisColor = vec3(0.85, 0.20, 0.20);
    vec3 zAxisColor = vec3(0.20, 0.45, 0.95);

    float axisWidth = 0.001;

    vec3 color = gridColor;

    if (abs(WorldPos.z) < axisWidth)
        color = xAxisColor;

    if (abs(WorldPos.x) < axisWidth)
        color = zAxisColor;

    float dist = distance(cameraPos.xz, WorldPos.xz);
    float fade = 1.0 - smoothstep(6.0, 18.0, dist);

    FragColor = vec4(color, fade);
}
)";

	const char* vertexSL = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vUV = aUV;

    gl_Position = uProjection * uView * worldPos;
}
)";
	const char* fragmentSL = R"(
#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uKd;
uniform float uD;

uniform sampler2D uDiffuseMap;
uniform int uHasDiffuseTexture;

void main()
{
    vec4 baseColor = vec4(uKd, uD);

    if (uHasDiffuseTexture == 1)
    {
        baseColor *= texture(uDiffuseMap, vUV);
    }

    FragColor = baseColor;
}
)";

private:
	ViewController viewCtrl;
	ShaderCompiler shader;
	GLuint gridProgram = 0;
	GLuint modelProgram = 0;
	GridObj grid;
	ObjLoader objloader;
	glm::mat4 model{ 1 };

	float fov = 45.0;
	glm::mat4 projection = glm::perspective(
		glm::radians(fov),
		800.0f / 600.0f,
		0.1f,
		10000.0f
	);
public:
	std::string rspath = "D:/Projects/c++/FractalDumpset/Resources/";
	void SetResourcesPath(std::string rspath)
	{
		this->rspath = rspath+"/";
		printf("rspath:%s\n", rspath.c_str());
	}
	void Init() override
	{
		grid.InitGrid(10, 200);

		objloader.LoadObj(rspath + "zeraora/zeraora.obj", rspath + "zeraora/zeraora.mtl");
		objloader.CreateVAOVBO();

		gridProgram = shader.CompileShaderGLSL(gridVertexSL, gridFragmentSL);
		modelProgram = shader.CompileShaderGLSL(vertexSL, fragmentSL);
		viewCtrl.SetCamera(
			glm::vec3(-2.0f, 0.0f, 0.0f),//相机位置
			glm::vec3(0.0f, 0.0f, 0.0f),//朝向世界坐标
			glm::vec3(0.0f, 1.0f, 0.0f));//上方向
	}
	void Render(GLuint fbo) override
	{
		glm::mat4 view = viewCtrl.GetNowView();
		glm::vec3 cameraPos = viewCtrl.GetNowCameraPos();

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);//在render提供的fbo上画
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		grid.Draw(gridProgram, model, view, projection, cameraPos);
		objloader.Draw(modelProgram, model, view, projection, cameraPos);
	}
	void Resize() override
	{
		auto bounds = GetBounds();
		float aspect = bounds.w / bounds.h;
		projection = glm::perspective(
			glm::radians(fov),
			aspect,
			0.1f,
			10000.0f
		);
	}
};

class MyRootComponent :public Enola2::Component, public Enola2::EventListener
{
private:
	std::string rspath = "";
	Model v3d;
public:
	MyRootComponent()
	{
		AddChild(v3d);
		rspath = Resources::UnpackResources();
		v3d.SetResourcesPath(rspath);
	}
	~MyRootComponent()
	{
		Resources::ClearTmpResources(rspath);
	}
	void Init() override
	{
	}
	void Render(GLuint fbo) override
	{

	}
	void Resize() override
	{
		auto bounds = GetBounds();
		printf("Root Bounds:%d %d %d %d\n", (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);
		v3d.SetBounds({ 0,64,bounds.w,bounds.h - 128 });
	}
};