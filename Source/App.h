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


class MyRootComponent :public Enola2::Component, public Enola2::EventListener
{
private:

#ifdef USE_PACK_RESOURCES
	std::string rspath = "";
#endif 

	SceneEditor v3d;
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