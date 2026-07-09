#pragma once

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

#include "E3dUtils.h"
#include "E3dModel.h"
#include "Enola2Event.h"
#include "Enola2Component.h"

class Scene :public Model
{
private:
	struct TagModel
	{
		std::unique_ptr<Model> model;
		std::string indexName;
	};
	Model nullModel;
	TagModel nullTagModel{ std::unique_ptr<Model>(&nullModel),"null" };
	std::vector<TagModel> models;
public:
	void AddModel(Model& model, std::string indexName)
	{
		models.push_back({ std::unique_ptr<Model>(&model), indexName });
	}
	Model& GetModelRef(std::string indexName)
	{
		if (indexName == "RootScene")return *this;
		for (auto& m : models)if (m.indexName == indexName) return *m.model;
		return nullModel;
	}

	void Draw(GLuint shaderProgram,//着色器程序
		const glm::mat4 view,//视图
		const glm::mat4 projection,//投影
		const glm::vec3 cameraPos,
		const glm::mat4 parentModel = { 1 }) override//相机坐标
	{
		glm::mat4 finalModel = parentModel * model;
		for (auto& m : models)
		{
			m.model->Draw(shaderProgram, view, projection, cameraPos, finalModel);
		}
	}

	float IsPointToModel(
		glm::vec2 pointPos2d,
		glm::vec2 viewportSize,
		glm::mat4 view,
		glm::mat4 projection
	) override
	{
		float nearestDepth = (std::numeric_limits<float>::max)();
		for (auto& m : models)
		{
			float dp = m.model->IsPointToModel(pointPos2d, viewportSize, view, projection);
			if (dp < nearestDepth)
			{
				nearestDepth = dp;
			}
		}
		return nearestDepth;
	}

	TagModel& GetPointToModelRef(
		glm::vec2 pointPos2d,
		glm::vec2 viewportSize,
		glm::mat4 view,
		glm::mat4 projection
	)
	{
		TagModel* md = &nullTagModel;
		float nearestDepth = (std::numeric_limits<float>::max)();
		for (auto& m : models)
		{
			float dp = m.model->IsPointToModel(pointPos2d, viewportSize, view, projection);
			if (dp < nearestDepth)
			{
				nearestDepth = dp;
				md = &m;
			}
		}
		return *md;
	}
};

//变换器，类似blender的
//需要：组件系统坐标（解耦鼠标指针位置），fbo进行绘制，模型变换矩阵等。
//如果不进行细致的抽象，这个类有可能承载比较重的语义。
class TransformGizmo : public Scene
{
private:
public:
};

class SceneTest1 :public Scene
{
private:
	std::string rspath = "D:/Projects/c++/FractalDumpset/Resources/";
	ObjModel zeraora;
public:
	void Init() override
	{
		zeraora.LoadObj(rspath + "zeraora/zeraora.obj", rspath + "zeraora/zeraora.mtl");
		zeraora.CreateVAOVBO();
		AddModel(zeraora, "zeraora1");
	}
};

class SceneEditor :public Enola2::Component, public Enola2::EventListener
{
private:
	const char* vertexSL = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;   // xyz=tangent, w=handedness
layout(location = 4) in vec4 aColor;     // vertex color
layout(location = 5) in vec2 aUV1;       // second uv
layout(location = 6) in ivec4 aJoints;   // skinning joints
layout(location = 7) in vec4 aWeights;   // skinning weights

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

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

	GridModel grid;//网格
	SceneTest1 scene;//场景
	TransformGizmo gizmo;//操纵器
	int enableGizmo = 0;

	GLuint shaderProgram = 0;

	glm::mat4 model{ 1 };
	float fov = 45.0;
	glm::mat4 projection = glm::perspective(
		glm::radians(fov),
		800.0f / 600.0f,
		0.1f,
		10000.0f
	);

public://鼠标键盘操作相关
	void OnMouse(const Enola2::MouseEvent& e)override
	{
		if (e.state == Enola2::MouseState::LDoubleClick)
		{
			auto [px, py] = GlobalPosToComponent(e.x, e.y);
			glm::vec2 point = { px, py };
			glm::vec2 viewportSize = { GetBounds().w,GetBounds().h };
			glm::mat4 view = viewCtrl.GetNowView();
			auto& pointRef = scene.GetPointToModelRef(point, viewportSize, view, projection);
			printf("Double click: [ModelName] %s\n", pointRef.indexName.c_str());
		}
	}
	void OnKey(const Enola2::KeyEvent& e)override
	{
	}

public://组件相关

	void Init() override
	{
		Enola2::EventListener::Register(this);

		viewCtrl.SetCamera(
			glm::vec3(-2.0f, 0.0f, 0.0f),//相机位置
			glm::vec3(0.0f, 0.0f, 0.0f),//朝向世界坐标
			glm::vec3(0.0f, 1.0f, 0.0f));//上方向

		//还需初始化shader program，等明天把shader封装好。
		scene.Init();
		grid.Init();

		shaderProgram = ShaderCompiler::CompileShaderGLSL(vertexSL, fragmentSL);
	}
	void Render(GLuint fbo) override
	{
		glm::mat4 view = viewCtrl.GetNowView();
		glm::vec3 cameraPos = viewCtrl.GetNowCameraPos();

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);//在render提供的fbo上画
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		grid.Draw(shaderProgram, view, projection, cameraPos, { 1.0 });
		scene.Draw(shaderProgram, view, projection, cameraPos, { 1.0 });
		if (enableGizmo)gizmo.Draw(shaderProgram, view, projection, cameraPos, { 1.0 });
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