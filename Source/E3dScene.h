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
	Model nullModel;
	struct TagModel
	{
		std::unique_ptr<Model> model;
		std::string indexName;
	};
	std::vector<TagModel> models;
public:
	void AddModel(Model* model, std::string indexName)
	{
		models.push_back({ std::unique_ptr<Model>(model), indexName });
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

	Model& GetPointToModelRef(
		glm::vec2 pointPos2d,
		glm::vec2 viewportSize,
		glm::mat4 view,
		glm::mat4 projection
	)
	{
		Model* md = &nullModel;
		float nearestDepth = (std::numeric_limits<float>::max)();
		for (auto& m : models)
		{
			float dp = m.model->IsPointToModel(pointPos2d, viewportSize, view, projection);
			if (dp < nearestDepth)
			{
				nearestDepth = dp;
				md = m.model.get();
			}
		}
		return *md;
	}
};

//变换器，类似blender的
class TransformGizmo : public Scene
{
private:
public:
	void SetModelMatrix(glm::mat4 mModel)//设置模型变换矩阵
	{

	}
};

class SceneEditor :public Enola2::Component, public Enola2::EventListener
{
private:
	ViewController viewCtrl;

	GridModel grid;//网格
	Scene scene;//场景
	TransformGizmo gizmo;//操纵器


public://鼠标键盘操作相关
	void OnMouse(const Enola2::MouseEvent& e)override {}
	void OnKey(const Enola2::KeyEvent& e)override {}

public://组件相关

	void Init() override
	{
		Enola2::EventListener::Register(this);

	}
	void Render(GLuint fbo) override
	{
		glm::mat4 view = viewCtrl.GetNowView();
		glm::vec3 cameraPos = viewCtrl.GetNowCameraPos();

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);//在render提供的fbo上画
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	void Resize() override
	{

	}

};