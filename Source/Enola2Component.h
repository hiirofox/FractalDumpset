#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include "GLLoader.h"
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
	}; class RenderContext2
	{
	private:
		struct Target
		{
			GLuint fbo = 0;
			GLuint color = 0;
			GLuint depth = 0;
			int w = 0;
			int h = 0;
		};

		static inline Target target;
		static inline Rectf currentBounds{ 0,0,0,0 };

	private:
		static void EnsureTarget(int w, int h)
		{
			if (w <= 0 || h <= 0)
				return;

			if (target.fbo != 0 && target.w == w && target.h == h)
				return;

			if (target.fbo)
			{
				glDeleteFramebuffers(1, &target.fbo);
				glDeleteTextures(1, &target.color);
				glDeleteTextures(1, &target.depth);
			}

			target.w = w;
			target.h = h;

			glGenFramebuffers(1, &target.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

			// color attachment
			glGenTextures(1, &target.color);
			glBindTexture(GL_TEXTURE_2D, target.color);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGBA8,
				w,
				h,
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				nullptr
			);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				target.color,
				0
			);

			// depth attachment
			glGenTextures(1, &target.depth);
			glBindTexture(GL_TEXTURE_2D, target.depth);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_DEPTH_COMPONENT24,
				w,
				h,
				0,
				GL_DEPTH_COMPONENT,
				GL_FLOAT,
				nullptr
			);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
				GL_DEPTH_ATTACHMENT,
				GL_TEXTURE_2D,
				target.depth,
				0
			);

			GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
			glDrawBuffers(1, bufs);

			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				printf("RenderContext2 FBO incomplete: 0x%x\n", status);
			}

			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		static void ResetState()
		{
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
		static GLuint StartRender(Rectf globalBounds)
		{
			currentBounds = globalBounds;

			int w = (int)globalBounds.w;
			int h = (int)globalBounds.h;

			EnsureTarget(w, h);
			ResetState();

			glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

			GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
			glDrawBuffers(1, bufs);

			// 关键：Render() 里面是干净的 0,0,w,h
			glViewport(0, 0, w, h);

			glEnable(GL_SCISSOR_TEST);
			glScissor(0, 0, w, h);

			return target.fbo;
		}

		static void EndRender()
		{
			int x = (int)currentBounds.x;
			int y = (int)currentBounds.y;
			int w = (int)currentBounds.w;
			int h = (int)currentBounds.h;

			// 从组件 framebuffer 读
			glBindFramebuffer(GL_READ_FRAMEBUFFER, target.fbo);
			glReadBuffer(GL_COLOR_ATTACHMENT0);

			// 写回默认 framebuffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			glEnable(GL_SCISSOR_TEST);
			glScissor(x, y, w, h);

			glBlitFramebuffer(
				0, 0, target.w, target.h,
				x, y, x + w, y + h,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			glDepthMask(GL_TRUE);
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
			auto frameBuffer = RenderContext2::StartRender({ directX, directY, bounds.w, bounds.h });
			Render(frameBuffer);//绘制自己的
			RenderContext2::EndRender();
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
		virtual void Render(GLuint frameBuffer)
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
		Rectf GetLocalBounds()
		{
			return { directX,directY,bounds.w,bounds.h };
		}
		std::pair<float, float> GlobalPosToComponent(float x, float y)
		{
			return { x - directX,y - directY };
		}
	};
}
