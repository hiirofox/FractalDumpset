#pragma once

#include <windows.h>

#undef USE_PACK_RESOURCES
#ifdef USE_PACK_RESOURCES
#include "../Resources_Resources.rsh"
#endif

//#include "Test3DObj.h"
//#include "TestTerrain.h"

#include "E3dModel.h"
#include "E3dShader.h"
#include "E3dScene.h"


class TestObjModel :public Enola2::Component
{
private://shader
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
	ShaderCompiler shader;
	GLuint shaderProgram = 0;
	GridModel grid;
	ObjModel objloader;
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
		this->rspath = rspath + "/";
		printf("rspath:%s\n", rspath.c_str());
	}
	void Init() override
	{
		grid.Init();

		objloader.LoadObj(rspath + "zeraora/zeraora.obj", rspath + "zeraora/zeraora.mtl");
		objloader.CreateVAOVBO();

		shaderProgram = shader.CompileShaderGLSL(vertexSL, fragmentSL);
		viewCtrl.SetCamera(
			glm::vec3(-2.0f, 0.0f, 0.0f),//相机位置
			glm::vec3(0.0f, 0.0f, 0.0f),//朝向世界坐标
			glm::vec3(0.0f, 1.0f, 0.0f));//上方向
	}
	float t = 0.0;
	void Render(GLuint fbo) override
	{
		glm::mat4 view = viewCtrl.GetNowView();
		glm::vec3 cameraPos = viewCtrl.GetNowCameraPos();

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);//在render提供的fbo上画
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		objloader.ResetModel();
		objloader.RotateModel({ 0.0,t * 360.0,0.0 });
		t += 0.001;
		t -= (int)t;

		grid.Draw(shaderProgram, view, projection, cameraPos);
		objloader.Draw(shaderProgram, view, projection, cameraPos);
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

#ifdef USE_PACK_RESOURCES
	std::string rspath = "";
#endif 

	TestObjModel v3d;
public:
	MyRootComponent()
	{
		AddChild(v3d);
#ifdef USE_PACK_RESOURCES
		rspath = Resources::UnpackResources();
		v3d.SetResourcesPath(rspath);
#endif
	}
	~MyRootComponent()
	{
#ifdef USE_PACK_RESOURCES
		Resources::ClearTmpResources(rspath);
#endif
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