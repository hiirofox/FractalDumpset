#pragma once

#include <numeric>

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

#include "E3dDef.h"
#include "E3dUtils.h"

class ModelComponent
{
private:
	std::vector<ModelComponent*> children;
	GLuint shader = 0;
	glm::mat4 model = { 1.0 };
	std::string name = "model";
private:
	void DoDraw(CameraContext& cctx)
	{
		Draw(cctx);
		for (auto* c : children)c->DoDraw(cctx);
	}
	void DoUpdateShader(GLuint shader)
	{
		this->shader = shader;
		for (auto* c : children)c->UpdateShader(shader);
	}
	void DoUpdateTransform(glm::mat4 model)
	{
		this->model = model;
		for (auto* c : children)c->UpdateTransform(model);
	}
	void DoUpdateTick(double t)
	{
		Tick(t);
		for (auto* c : children)c->DoUpdateTick(t);
	}
public:
	void AddChildModel(ModelComponent& child)
	{
		if (&child != this)children.push_back(&child);
	}

	//gl初始化之后执行
	virtual void Init() {}

	//绘制模型
	virtual void Draw(CameraContext& cctx) {}
	//重绘，一般是根组件调用
	void ReDraw(CameraContext& cctx) { DoDraw(cctx); }

	//父组件更新shader，里面可以对子组件SetShader
	virtual void UpdateShader(GLuint shader) {}
	//设置shader，会触发子组件UpdateShader
	void SetShader(GLuint shader) { DoUpdateShader(shader); }
	GLuint GetShader() { return shader; }

	//父组件更新model时调用，里面可以对子组件SetTransform
	virtual void UpdateTransform(glm::mat4 parentModel) {}
	//设置model矩阵，会触发子组件UpdateTransform
	void SetTransform(glm::mat4 model) { DoUpdateTransform(model); }
	glm::mat4 GetModelMatrix() { return model; }

	//根据系统tick更新触发
	virtual void Tick(double t) {}
	//手动触发tick，一般是根组件调用
	void TrigTick(double t) { DoUpdateTick(t); }

public: //实用接口，非必须实现

	//模型内部自己写射线检测算法。返回深度
	virtual std::pair<float, ModelComponent*> RayCheckMethod(
		glm::vec2 pointPos2d,  //绘制坐标
		glm::vec2 viewportSize,//绘制区域大小
		CameraContext& cctx
	)
	{
		return { (std::numeric_limits<float>::max)() ,NULL };
	}
	std::pair<float, ModelComponent*> RayCheck(
		glm::vec2 pointPos2d,
		glm::vec2 viewportSize,
		CameraContext& cctx
	)
	{
		float minz = (std::numeric_limits<float>::max)();
		std::pair<float, ModelComponent*> rayTo = { minz,NULL };
		auto check = RayCheckMethod(pointPos2d, viewportSize, cctx);
		if (check.first < rayTo.first)rayTo = check;
		for (auto* child : children)
		{
			auto check = child->RayCheck(pointPos2d, viewportSize, cctx);
			if (check.first < rayTo.first)rayTo = check;
		}
		return rayTo;
	}

	void SetName(std::string name) { this->name = name; }
	std::string GetName() { return name; }
};

class ObjModel :public ModelComponent
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
		SetName("ObjModel");
	}

	void Draw(CameraContext& cctx) override
	{
		glm::mat4 finalModel = GetModelMatrix();
		GLuint shaderProgram = GetShader();
		auto view = cctx.view;
		auto projection = cctx.projection;
		auto cameraPos = cctx.GetCameraPos();

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
			glm::value_ptr(finalModel)
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

	static bool RayTriangleIntersect(
		const glm::vec3& rayOrigin,
		const glm::vec3& rayDir,
		const glm::vec3& v0,
		const glm::vec3& v1,
		const glm::vec3& v2,
		float& t
	)
	{
		const float EPS = 0.000001f;

		glm::vec3 edge1 = v1 - v0;
		glm::vec3 edge2 = v2 - v0;

		glm::vec3 pvec = glm::cross(rayDir, edge2);
		float det = glm::dot(edge1, pvec);

		if (fabs(det) < EPS)
			return false;

		float invDet = 1.0f / det;

		glm::vec3 tvec = rayOrigin - v0;
		float u = glm::dot(tvec, pvec) * invDet;

		if (u < 0.0f || u > 1.0f)
			return false;

		glm::vec3 qvec = glm::cross(tvec, edge1);
		float v = glm::dot(rayDir, qvec) * invDet;

		if (v < 0.0f || u + v > 1.0f)
			return false;

		t = glm::dot(edge2, qvec) * invDet;

		return t >= 0.0f;
	}

	std::pair<float, ModelComponent*> RayCheckMethod(
		glm::vec2 pointPos2d,  //绘制坐标
		glm::vec2 viewportSize,//绘制区域大小
		CameraContext& cctx
	) override
	{
		glm::mat4 finalModel = GetModelMatrix();
		GLuint shaderProgram = GetShader();
		auto view = cctx.view;
		auto projection = cctx.projection;
		auto cameraPos = cctx.GetCameraPos();


		if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
			return { (std::numeric_limits<float>::max)() ,NULL };

		glm::mat4 model = GetModelMatrix();

		float ndcX = (pointPos2d.x / viewportSize.x) * 2.0f - 1.0f;
		float ndcY = 1.0f - (pointPos2d.y / viewportSize.y) * 2.0f;

		glm::mat4 invViewProjection = glm::inverse(projection * view);

		glm::vec4 nearClip(ndcX, ndcY, -1.0f, 1.0f);
		glm::vec4 farClip(ndcX, ndcY, 1.0f, 1.0f);

		glm::vec4 nearWorld4 = invViewProjection * nearClip;
		glm::vec4 farWorld4 = invViewProjection * farClip;

		if (nearWorld4.w == 0.0f || farWorld4.w == 0.0f)
			return { (std::numeric_limits<float>::max)() ,NULL };

		glm::vec3 nearWorld = glm::vec3(nearWorld4) / nearWorld4.w;
		glm::vec3 farWorld = glm::vec3(farWorld4) / farWorld4.w;

		glm::vec3 rayOriginWorld = nearWorld;
		glm::vec3 rayDirWorld = glm::normalize(farWorld - nearWorld);

		// 关键：把世界空间射线变换到模型局部空间
		glm::mat4 invModel = glm::inverse(model);

		glm::vec3 rayOriginLocal =
			glm::vec3(invModel * glm::vec4(rayOriginWorld, 1.0f));

		glm::vec3 rayDirLocal =
			glm::normalize(glm::vec3(invModel * glm::vec4(rayDirWorld, 0.0f)));

		bool hit = false;
		float nearestDepth = (std::numeric_limits<float>::max)();

		for (size_t i = 0; i + 2 < vertices.size(); i += 3)
		{
			const glm::vec3& v0 = vertices[i + 0].pos;
			const glm::vec3& v1 = vertices[i + 1].pos;
			const glm::vec3& v2 = vertices[i + 2].pos;

			float tLocal = 0.0f;
			if (!RayTriangleIntersect(rayOriginLocal, rayDirLocal, v0, v1, v2, tLocal))
				continue;

			glm::vec3 hitLocal = rayOriginLocal + rayDirLocal * tLocal;

			// 命中点转回世界空间
			glm::vec3 hitWorld =
				glm::vec3(model * glm::vec4(hitLocal, 1.0f));

			// 再转 view space，返回相机深度
			glm::vec3 hitView =
				glm::vec3(view * glm::vec4(hitWorld, 1.0f));

			float depth = -hitView.z;

			if (depth >= 0.0f && depth < nearestDepth)
			{
				nearestDepth = depth;
				hit = true;
			}
		}

		return { nearestDepth, this };
	}

	void UpdateShader(GLuint shader) override
	{
		SetShader(shader);
	}
	void UpdateTransform(glm::mat4 parentModel)
	{
		SetTransform(parentModel * glm::mat4{ 1.0 });
	}
};
class GridModel : public ModelComponent
{
private:
	struct GridVertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
	};

private:
	GLuint gridVAO = 0;
	GLuint gridVBO = 0;

	GLuint gridEBO = 0;
	GLuint xAxisEBO = 0;
	GLuint zAxisEBO = 0;

	GLuint fadeTexture = 0;

	int gridIndexCount = 0;
	int xAxisIndexCount = 0;
	int zAxisIndexCount = 0;

	std::vector<GridVertex> gridVertices;

	float fadeStart = 6.0f;
	float fadeEnd = 18.0f;

private:
	static void SetMat4(GLuint program, const char* name, const glm::mat4& value)
	{
		GLint loc = glGetUniformLocation(program, name);
		if (loc >= 0)
			glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
	}

	static void SetVec3(GLuint program, const char* name, const glm::vec3& value)
	{
		GLint loc = glGetUniformLocation(program, name);
		if (loc >= 0)
			glUniform3fv(loc, 1, glm::value_ptr(value));
	}

	static void SetFloat(GLuint program, const char* name, float value)
	{
		GLint loc = glGetUniformLocation(program, name);
		if (loc >= 0)
			glUniform1f(loc, value);
	}

	static void SetInt(GLuint program, const char* name, int value)
	{
		GLint loc = glGetUniformLocation(program, name);
		if (loc >= 0)
			glUniform1i(loc, value);
	}

	static float SmoothStep(float edge0, float edge1, float x)
	{
		float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	void CreateFadeTexture(int size = 512)
	{
		std::vector<unsigned char> pixels(size * size * 4);

		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				float u = ((float)x + 0.5f) / (float)size;
				float v = ((float)y + 0.5f) / (float)size;

				float dx = (u - 0.5f) * 2.0f * fadeEnd;
				float dz = (v - 0.5f) * 2.0f * fadeEnd;

				float dist = sqrtf(dx * dx + dz * dz);
				float fade = 1.0f - SmoothStep(fadeStart, fadeEnd, dist);

				unsigned char alpha = (unsigned char)glm::clamp(fade * 255.0f, 0.0f, 255.0f);

				int index = (y * size + x) * 4;

				pixels[index + 0] = 255;
				pixels[index + 1] = 255;
				pixels[index + 2] = 255;
				pixels[index + 3] = alpha;
			}
		}

		glGenTextures(1, &fadeTexture);
		glBindTexture(GL_TEXTURE_2D, fadeTexture);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			size,
			size,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixels.data()
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void UpdateFadeUV(const glm::vec3& cameraPos)
	{
		float invRange = 1.0f / (2.0f * fadeEnd);
		glm::mat4 model = GetModelMatrix();

		for (GridVertex& v : gridVertices)
		{
			glm::vec3 worldPos = glm::vec3(model * glm::vec4(v.pos, 1.0f));

			v.uv.x = 0.5f + (worldPos.x - cameraPos.x) * invRange;
			v.uv.y = 0.5f + (worldPos.z - cameraPos.z) * invRange;
		}

		glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			gridVertices.size() * sizeof(GridVertex),
			gridVertices.data()
		);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void UploadIndexBuffer(GLuint& ebo, const std::vector<unsigned int>& indices)
	{
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(unsigned int),
			indices.empty() ? nullptr : indices.data(),
			GL_STATIC_DRAW
		);
	}

	void DrawIndexBatch(GLuint shaderProgram, GLuint ebo, int indexCount, const glm::vec3& color)
	{
		if (indexCount <= 0)
			return;

		SetVec3(shaderProgram, "uKd", color);
		SetFloat(shaderProgram, "uD", 1.0f);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glDrawElements(GL_LINES, indexCount, GL_UNSIGNED_INT, 0);
	}

public:
	void InitGrid(float halfSize = 10.0f, int lines = 200)
	{
		SetName("Grid");

		gridVertices.clear();

		std::vector<unsigned int> gridIndices;
		std::vector<unsigned int> xAxisIndices;
		std::vector<unsigned int> zAxisIndices;

		float step = (halfSize * 2.0f) / (float)lines;

		int axisIndex = (int)roundf(halfSize / step);
		bool hasExactAxis = fabsf((-halfSize + axisIndex * step)) < 0.00001f;

		for (int z = 0; z <= lines; ++z)
		{
			for (int x = 0; x <= lines; ++x)
			{
				float px = -halfSize + x * step;
				float pz = -halfSize + z * step;

				GridVertex vertex;
				vertex.pos = glm::vec3(px, 0.0f, pz);
				vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
				vertex.uv = glm::vec2(0.0f, 0.0f);

				gridVertices.push_back(vertex);
			}
		}

		// 横向线：沿 X 方向，z == 0 的这一排就是 X 轴
		for (int z = 0; z <= lines; ++z)
		{
			for (int x = 0; x < lines; ++x)
			{
				unsigned int i0 = z * (lines + 1) + x;
				unsigned int i1 = i0 + 1;

				if (hasExactAxis && z == axisIndex)
				{
					xAxisIndices.push_back(i0);
					xAxisIndices.push_back(i1);
				}
				else
				{
					gridIndices.push_back(i0);
					gridIndices.push_back(i1);
				}
			}
		}

		// 纵向线：沿 Z 方向，x == 0 的这一列就是 Z 轴
		for (int z = 0; z < lines; ++z)
		{
			for (int x = 0; x <= lines; ++x)
			{
				unsigned int i0 = z * (lines + 1) + x;
				unsigned int i1 = i0 + (lines + 1);

				if (hasExactAxis && x == axisIndex)
				{
					zAxisIndices.push_back(i0);
					zAxisIndices.push_back(i1);
				}
				else
				{
					gridIndices.push_back(i0);
					gridIndices.push_back(i1);
				}
			}
		}

		gridIndexCount = (int)gridIndices.size();
		xAxisIndexCount = (int)xAxisIndices.size();
		zAxisIndexCount = (int)zAxisIndices.size();

		glGenVertexArrays(1, &gridVAO);
		glGenBuffers(1, &gridVBO);

		glBindVertexArray(gridVAO);

		glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
		glBufferData(
			GL_ARRAY_BUFFER,
			gridVertices.size() * sizeof(GridVertex),
			gridVertices.data(),
			GL_DYNAMIC_DRAW
		);

		// layout(location = 0) in vec3 aPos;
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(GridVertex),
			(void*)offsetof(GridVertex, pos)
		);

		// layout(location = 1) in vec3 aNormal;
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,
			3,
			GL_FLOAT,
			GL_FALSE,
			sizeof(GridVertex),
			(void*)offsetof(GridVertex, normal)
		);

		// layout(location = 2) in vec2 aUV;
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(
			2,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(GridVertex),
			(void*)offsetof(GridVertex, uv)
		);

		UploadIndexBuffer(gridEBO, gridIndices);
		UploadIndexBuffer(xAxisEBO, xAxisIndices);
		UploadIndexBuffer(zAxisEBO, zAxisIndices);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		CreateFadeTexture(512);
	}

	void Draw(CameraContext& cctx) override
	{
		glm::mat4 finalModel = GetModelMatrix();
		GLuint shaderProgram = GetShader();
		auto view = cctx.view;
		auto projection = cctx.projection;
		auto cameraPos = cctx.GetCameraPos();

		UpdateFadeUV(cameraPos);

		glUseProgram(shaderProgram);

		SetMat4(shaderProgram, "uModel", finalModel);
		SetMat4(shaderProgram, "uView", view);
		SetMat4(shaderProgram, "uProjection", projection);

		glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(finalModel)));
		GLint normalLoc = glGetUniformLocation(shaderProgram, "uNormalMatrix");
		if (normalLoc >= 0)
			glUniformMatrix3fv(normalLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fadeTexture);

		SetInt(shaderProgram, "uDiffuseMap", 0);
		SetInt(shaderProgram, "uHasDiffuseTexture", 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBindVertexArray(gridVAO);

		// 普通网格
		DrawIndexBatch(
			shaderProgram,
			gridEBO,
			gridIndexCount,
			glm::vec3(0.32f, 0.32f, 0.32f)
		);

		// X 轴，红色
		DrawIndexBatch(
			shaderProgram,
			xAxisEBO,
			xAxisIndexCount,
			glm::vec3(0.85f, 0.20f, 0.20f)
		);

		// Z 轴，蓝色
		DrawIndexBatch(
			shaderProgram,
			zAxisEBO,
			zAxisIndexCount,
			glm::vec3(0.20f, 0.45f, 0.95f)
		);

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Init() override
	{
		InitGrid(10.0f, 200);
	}

	void UpdateShader(GLuint shader) override
	{
		SetShader(shader);
	}
	void UpdateTransform(glm::mat4 parentModel)
	{
		SetTransform(parentModel * glm::mat4{ 1.0 });
	}

	~GridModel()
	{
		if (gridVBO != 0)
			glDeleteBuffers(1, &gridVBO);

		if (gridEBO != 0)
			glDeleteBuffers(1, &gridEBO);

		if (xAxisEBO != 0)
			glDeleteBuffers(1, &xAxisEBO);

		if (zAxisEBO != 0)
			glDeleteBuffers(1, &zAxisEBO);

		if (gridVAO != 0)
			glDeleteVertexArrays(1, &gridVAO);

		if (fadeTexture != 0)
			glDeleteTextures(1, &fadeTexture);
	}
};