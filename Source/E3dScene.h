#pragma once

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "External/glm/glm/gtx/euler_angles.hpp"

#include "E3dUtils.h"
#include "E3dModel.h"
#include "Enola2Event.h"
#include "Enola2Component.h"
#include "Enola2Timer.h"

//变换器，类似blender的
//需要：组件系统坐标（解耦鼠标指针位置），fbo进行绘制，模型变换矩阵等。
//如果不进行细致的抽象，这个类有可能承载比较重的语义。
class TransformGizmo : public ModelComponent
{
private:
public:
};

class Zeraora :public ModelComponent
{
private:
	std::string rspath = "D:/Projects/c++/FractalDumpset/Resources/";
	ObjModel zeraora;
public:
	void Init() override
	{
		zeraora.LoadObj(rspath + "zeraora/zeraora.obj", rspath + "zeraora/zeraora.mtl");
		zeraora.CreateVAOVBO();
		SetName("zeraora");
		AddChildModel(zeraora);
	}
	double TimeToCycle(double t, double k)
	{
		t *= k;
		return t - floor(t);
	}
	void Tick(double ts) override
	{
		float cycle = TimeToCycle(ts, 0.05);
		zeraora.SetTransform(glm::eulerAngleXYZ(
			glm::radians(0.0),
			glm::radians(cycle * 360.0),
			glm::radians(0.0)));
	}
};

class SceneEditor :public Enola2::Component, public Enola2::EventListener, public Enola2::Timer
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

	Zeraora scene;//场景

	GridModel grid;//网格
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
			CameraContext cctx = { view, projection };
			auto [depth, model] = scene.RayCheck(point, viewportSize, cctx);
			std::string modelName = "null";
			if (model) modelName = model->GetName();
			printf("Double click: [ModelName] %s\n", modelName.c_str());
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

		scene.SetShader(shaderProgram);
		grid.SetShader(shaderProgram);
		gizmo.SetShader(shaderProgram);

		Enola2::GetProgramElapsedSeconds();
	}
	void Render(GLuint fbo) override
	{
		glm::mat4 view = viewCtrl.GetNowView();
		glm::vec3 cameraPos = viewCtrl.GetNowCameraPos();

		double ts = Enola2::GetProgramElapsedSeconds();
		grid.TrigTick(ts);
		scene.TrigTick(ts);
		gizmo.TrigTick(ts);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);//在render提供的fbo上画
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		CameraContext cctx = { view, projection };
		grid.ReDraw(cctx);//根组件调用绘制用ReDraw
		scene.ReDraw(cctx);
		if (enableGizmo)gizmo.ReDraw(cctx);
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