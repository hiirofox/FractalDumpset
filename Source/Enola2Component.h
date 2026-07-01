#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include "External/glad/include/glad/glad.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

namespace Enola2
{
	struct Rectf
	{
		float x, y, w, h;
	};
	class RenderContext
	{
	private:
		static void ResetState()
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			glActiveTexture(GL_TEXTURE0);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glDisable(GL_STENCIL_TEST);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glLineWidth(1.0f);
			glEnable(GL_SCISSOR_TEST);
		}

	public:
		static void StartRender(Rectf globalBounds)
		{
			ResetState();
			glViewport(
				(GLint)globalBounds.x,
				(GLint)globalBounds.y,
				(GLsizei)globalBounds.w,
				(GLsizei)globalBounds.h
			);
			glEnable(GL_SCISSOR_TEST);
			glScissor(
				(GLint)globalBounds.x,
				(GLint)globalBounds.y,
				(GLsizei)globalBounds.w,
				(GLsizei)globalBounds.h
			);
		}
		static void EndRender()
		{
			ResetState();
		}
	};
	class Component
	{
	private:
		float directX = 0, directY = 0;//全局坐标x,y
		Rectf bounds{ 0,0,100,100 };//相对于父组件的x,y，以及自己的w,h

		Component* father = NULL;
		std::vector<Component*> children;

	public://这些由外部友元，或者根组件(平台兼容层)管理
		void DoRender()
		{
			RenderContext::StartRender({ directX, directY, bounds.w, bounds.h });
			Render();//绘制自己的
			RenderContext::EndRender();
			for (auto* c : children)c->DoRender();//绘制子组件
		}
		void DoResize()
		{//如果调用这个，说明这个组件的大小是必须要变了
			if (father)//计算自己的绝对坐标
			{
				directX = father->directX + bounds.x;
				directY = father->directY + bounds.y;
			}
			else
			{
				directX = 0 + bounds.x;
				directY = 0 + bounds.y;
			}
			Resize();//通知用户，用户可以在Resize完成子组件的SetBounds
			for (auto* c : children)c->DoResize();//递归更新子控件的Resize
		}
		void DoInit()
		{
			Init();
			for (auto* c : children)c->DoInit();
		}
	public:
		virtual ~Component() {}

		virtual void Init()//初始化gl之后应该会被调用，此时可以进行gl相关的init
		{

		}
		void AddChild(Component& c)
		{
			children.push_back(&c);
			c.father = this;
		}
		virtual void Render()
		{

		}
		virtual void Resize()
		{

		}
		void SetBounds(Rectf bounds)
		{
			this->bounds = bounds;
			DoResize();
		}
		Rectf GetBounds()
		{
			return bounds;
		}
	};
}