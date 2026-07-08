#pragma once

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//用于读取图像数据的类
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

//用于模拟blender操作逻辑的类，返回view向量
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

