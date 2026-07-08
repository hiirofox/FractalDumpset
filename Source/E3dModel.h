#pragma once

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

#include "E3dUtils.h"

class Model
{
private:
public:
	//gl初始化之后执行
	virtual void Init() {}

	//绘制模型
	virtual void Draw(GLuint shaderProgram,//着色器程序
		const glm::mat4& view,//视图
		const glm::mat4& projection,//投影
		const glm::vec3& cameraPos) {//相机坐标
	}

	//模型摆放相关
	glm::mat4 model = { 1.0 };
	virtual void ResetModel()
	{
		model = { 1.0 };
	}
	virtual void SetModel(glm::mat4 m)
	{
		model = m;
	}
	virtual void SetModelPos(glm::vec3 p)
	{
		model[3] = glm::vec4(p, 1.0f);
	}
	virtual void TranslateModel(glm::vec3 t)
	{
		model = glm::translate(model, t);
	}
	virtual void ScaleModel(float k)
	{
		model = glm::scale(model, glm::vec3(k));
	}
	virtual void RotateModel(glm::vec3 r)
	{
		r = glm::radians(r);
		model = glm::rotate(model, r.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, r.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, r.z, glm::vec3(0.0f, 0.0f, 1.0f));
	}

};

class ObjModel :public Model
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

	//VAO/VBO
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
	void Init() override
	{
		CreateVAOVBO();
	}

	void Draw(GLuint shaderProgram,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPos) override
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

class GridModel :public Model
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
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPos) override
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

	//gl初始化之后执行
	void Init() override
	{
		InitGrid(10, 200);
	}

};