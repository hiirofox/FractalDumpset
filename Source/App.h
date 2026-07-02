#pragma once
#include <windows.h>

#include "Test3DObj.h"

#include <array>
#include <random>
#include <algorithm>
#include <cmath>
class PerlinNoise2D
{
private:
	std::array<int, 512> p;

	static float Fade(float t)
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	static float Lerp(float a, float b, float t)
	{
		return a + t * (b - a);
	}

	static float Grad(int hash, float x, float y)
	{
		switch (hash & 7)
		{
		case 0: return  x + y;
		case 1: return -x + y;
		case 2: return  x - y;
		case 3: return -x - y;
		case 4: return  x;
		case 5: return -x;
		case 6: return  y;
		case 7: return -y;
		default: return 0.0f;
		}
	}

public:
	PerlinNoise2D(unsigned int seed = 1337)
	{
		std::array<int, 256> perm;

		for (int i = 0; i < 256; i++)
			perm[i] = i;

		std::mt19937 rng(seed);
		std::shuffle(perm.begin(), perm.end(), rng);

		for (int i = 0; i < 512; i++)
			p[i] = perm[i & 255];
	}

	float Noise(float x, float y) const
	{
		int X = (int)floor(x) & 255;
		int Y = (int)floor(y) & 255;

		float xf = x - floor(x);
		float yf = y - floor(y);

		float u = Fade(xf);
		float v = Fade(yf);

		int aa = p[p[X] + Y];
		int ab = p[p[X] + Y + 1];
		int ba = p[p[X + 1] + Y];
		int bb = p[p[X + 1] + Y + 1];

		float x1 = Lerp(
			Grad(aa, xf, yf),
			Grad(ba, xf - 1.0f, yf),
			u
		);

		float x2 = Lerp(
			Grad(ab, xf, yf - 1.0f),
			Grad(bb, xf - 1.0f, yf - 1.0f),
			u
		);

		return Lerp(x1, x2, v); // 大约 [-1, 1]
	}

	float FBM(float x, float y, int octaves, float lacunarity, float persistence) const
	{
		float sum = 0.0f;
		float amp = 1.0f;
		float freq = 1.0f;
		float norm = 0.0f;

		for (int i = 0; i < octaves; i++)
		{
			sum += Noise(x * freq, y * freq) * amp;
			norm += amp;

			freq *= lacunarity;
			amp *= persistence;
		}

		return sum / norm; // [-1, 1]
	}

	float Ridged(float x, float y, int octaves, float lacunarity, float persistence) const
	{
		float sum = 0.0f;
		float amp = 1.0f;
		float freq = 1.0f;
		float norm = 0.0f;

		for (int i = 0; i < octaves; i++)
		{
			float n = Noise(x * freq, y * freq);
			n = 1.0f - fabs(n);
			n = n * n;

			sum += n * amp;
			norm += amp;

			freq *= lacunarity;
			amp *= persistence;
		}

		return sum / norm; // [0, 1]
	}
};


GLuint CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);

	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		char log[2048];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);

		if (type == GL_VERTEX_SHADER)
			printf("Vertex shader compile error:\n%s\n", log);
		else if (type == GL_FRAGMENT_SHADER)
			printf("Fragment shader compile error:\n%s\n", log);
		else
			printf("Shader compile error:\n%s\n", log);

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource)
{
	GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

	if (!vertexShader || !fragmentShader)
		return 0;

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glLinkProgram(program);

	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	if (!success)
	{
		char log[2048];
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		printf("Shader program link error:\n%s\n", log);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		glDeleteProgram(program);

		return 0;
	}

	// 链接成功后，shader object 可以删除
	// program 已经保存了最终链接结果
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return program;
}

class World :public Enola2::Component, public Enola2::EventListener
{
private://glsl 
	//顶点数据
	const char* terrainVertex = R"(
#version 330 core

layout(location = 0) in vec2 aXZ;
layout(location = 1) in vec2 aUV;

out vec2 vUV;
out vec3 vWorldPos;
out vec3 vNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D heightMap;
uniform float terrainSize;

float getHeight(vec2 uv)
{
    return texture(heightMap, uv).r;
}

vec3 getNormal(vec2 uv)
{
    vec2 texel = 1.0 / vec2(textureSize(heightMap, 0));

    float hL = getHeight(uv - vec2(texel.x, 0.0));
    float hR = getHeight(uv + vec2(texel.x, 0.0));
    float hD = getHeight(uv - vec2(0.0, texel.y));
    float hU = getHeight(uv + vec2(0.0, texel.y));

    float worldStep = terrainSize / float(textureSize(heightMap, 0).x - 1);

    return normalize(vec3(hL - hR, 2.0 * worldStep, hD - hU));
}

void main()
{
    float y = getHeight(aUV);

    vec3 localPos = vec3(aXZ.x, y, aXZ.y);

    vec4 world = model * vec4(localPos, 1.0);

    vWorldPos = world.xyz;
    vUV = aUV;

    mat3 normalMat = transpose(inverse(mat3(model)));
    vNormal = normalize(normalMat * getNormal(aUV));

    gl_Position = projection * view * world;
}
)";
	//着色器
	const char* terrainFragment = R"(
#version 330 core

in vec2 vUV;
in vec3 vWorldPos;
in vec3 vNormal;

out vec4 FragColor;

uniform vec3 cameraPos;

void main()
{
    vec3 normal = normalize(vNormal);

    vec3 lightDir = normalize(vec3(-0.3, 1.0, 0.4));

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 baseColor = mix(
        vec3(0.08, 0.35, 0.10),
        vec3(0.45, 0.38, 0.22),
        smoothstep(0.0, 5.0, vWorldPos.y)
    );

    vec3 color = baseColor * (0.25 + diff * 0.75);

    FragColor = vec4(color, 1.0);
}
)";
	GLuint terrainShader;
private://terrain 
	glm::mat4 model = glm::mat4(1.0f);

	//高度图
	GLint heightMapW = 512;
	GLint heightMapH = 512;
	std::vector<float> heightData;
	GLuint heightTex = 0;
	PerlinNoise2D terrainNoise{ 12345 };

	//地形网格数据
	int terrainIndexCount = 0;
	GLuint terrainVAO = 0;
	GLuint terrainVBO = 0;
	GLuint terrainEBO = 0;
	float terrainSize = 500.0;

	float Clamp01(float x)
	{
		if (x < 0.0f) return 0.0f;
		if (x > 1.0f) return 1.0f;
		return x;
	}

	float SmoothStep(float edge0, float edge1, float x)
	{
		x = Clamp01((x - edge0) / (edge1 - edge0));
		return x * x * (3.0f - 2.0f * x);
	}
	void InitHeightMap()//生成高度图
	{
		GLint w = heightMapW;
		GLint h = heightMapH;
		heightData.resize(w * h);

		for (int z = 0; z < h; z++)
		{
			for (int x = 0; x < w; x++)
			{
				float u = (float)x / (float)(w - 1);
				float v = (float)z / (float)(h - 1);

				float px = (u - 0.5f) * terrainSize;
				float pz = (v - 0.5f) * terrainSize;

				float continent = terrainNoise.FBM(
					px * 0.035f,
					pz * 0.035f,
					5,
					2.0f,
					0.5f
				);

				continent = continent * 0.5f + 0.5f;
				continent = SmoothStep(0.25f, 0.85f, continent);

				float mountainMask = SmoothStep(0.55f, 0.95f, continent);

				float hills = terrainNoise.FBM(
					px * 0.12f,
					pz * 0.12f,
					6,
					2.0f,
					0.48f
				) * 2.0f;

				float mountains = terrainNoise.Ridged(
					px * 0.055f,
					pz * 0.055f,
					6,
					2.1f,
					0.5f
				) * 12.0f * mountainMask;

				float detail = terrainNoise.FBM(
					px * 0.55f,
					pz * 0.55f,
					4,
					2.0f,
					0.45f
				) * 0.35f;

				float height = 0.0f;
				height += continent * 2.0f;
				height += hills;
				height += mountains;
				height += detail;
				height -= 1.0f;

				heightData[z * w + x] = height * 2.0;
			}
		}

		glGenTextures(1, &heightTex);
		glBindTexture(GL_TEXTURE_2D, heightTex);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_R32F,
			w,
			h,
			0,
			GL_RED,
			GL_FLOAT,
			heightData.data()
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
	void UploadHeightMap()
	{
		glBindTexture(GL_TEXTURE_2D, heightTex);

		glTexSubImage2D(
			GL_TEXTURE_2D,
			0,
			0,
			0,
			heightMapW,
			heightMapH,
			GL_RED,
			GL_FLOAT,
			heightData.data()
		);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
	void InitTerrain(float size, int cells)
	{
		struct TerrainVertex
		{
			glm::vec2 xz;
			glm::vec2 uv;
		};

		std::vector<TerrainVertex> vertices;
		std::vector<unsigned int> indices;

		int rowVertexCount = cells + 1;

		vertices.reserve(rowVertexCount * rowVertexCount);

		// =========================
		// 1. 生成平面顶点
		// =========================
		for (int z = 0; z <= cells; z++)
		{
			for (int x = 0; x <= cells; x++)
			{
				float u = (float)x / (float)cells;
				float v = (float)z / (float)cells;

				float px = (u - 0.5f) * size;
				float pz = (v - 0.5f) * size;

				TerrainVertex vert;
				vert.xz = glm::vec2(px, pz);
				vert.uv = glm::vec2(u, v);

				vertices.push_back(vert);
			}
		}

		// =========================
		// 2. 用 GL_TRIANGLE_STRIP 串起来
		// =========================
		// 每两行组成一条 triangle strip。
		// 行与行之间用 degenerate triangles 连接。
		for (int z = 0; z < cells; z++)
		{
			if (z > 0)
			{
				// degenerate bridge
				indices.push_back(z * rowVertexCount + cells);
				indices.push_back(z * rowVertexCount + 0);
			}

			for (int x = 0; x <= cells; x++)
			{
				int i0 = z * rowVertexCount + x;
				int i1 = (z + 1) * rowVertexCount + x;

				indices.push_back(i0);
				indices.push_back(i1);
			}
		}

		terrainIndexCount = (int)indices.size();

		// =========================
		// 3. 创建 VAO / VBO / EBO
		// =========================
		glGenVertexArrays(1, &terrainVAO);
		glGenBuffers(1, &terrainVBO);
		glGenBuffers(1, &terrainEBO);

		glBindVertexArray(terrainVAO);

		glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
		glBufferData(
			GL_ARRAY_BUFFER,
			vertices.size() * sizeof(TerrainVertex),
			vertices.data(),
			GL_STATIC_DRAW
		);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(unsigned int),
			indices.data(),
			GL_STATIC_DRAW
		);

		// layout(location = 0) in vec2 aXZ;
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(
			0,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(TerrainVertex),
			(void*)offsetof(TerrainVertex, xz)
		);

		// layout(location = 1) in vec2 aUV;
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(TerrainVertex),
			(void*)offsetof(TerrainVertex, uv)
		);

		glBindVertexArray(0);
	}
	void DrawTerrain()
	{
		glBindVertexArray(terrainVAO);

		glDrawElements(
			GL_TRIANGLE_STRIP,
			terrainIndexCount,
			GL_UNSIGNED_INT,
			0
		);

		glBindVertexArray(0);
	}
private://camera setting
	glm::vec3 cameraPos = glm::vec3(-10.0f, 20.0f, 0.0f);
	glm::vec3 cameraTarget = glm::vec3(0.0f, 20.0f, 0.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 view = glm::lookAt(
		cameraPos,  // 相机在“上方”
		cameraTarget,  // 看向原点
		cameraUp  // “上方向”
	);
	glm::mat4 projection = glm::perspective(
		glm::radians(45.0f),
		800.0f / 600.0f,
		0.1f,
		10000.0f
	);
public://处理camera操作逻辑

	bool middleMouseDown = false;
	bool shiftDown = false;

	float lastMouseX = 0.0f;
	float lastMouseY = 0.0f;

	float orbitSpeed = 0.005f;
	float panSpeed = 0.0015f;

	float wheelZoomFactor = 88.0f;      // 每滚一格缩放倍率，小于 1 表示靠近
	float minZoomDistance = 1.0f;       // 离 target 最近距离
	float maxZoomDistance = 2000.0f;    // 离 target 最远距离

	void UpdateCameraView()
	{
		view = glm::lookAt(
			cameraPos,
			cameraTarget,
			cameraUp
		);
	}

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


private://time
	float t = 0.0;
public:

	void Init() override
	{
		Enola2::EventListener::Register(this);
		terrainShader = CreateShaderProgram(terrainVertex, terrainFragment);
		InitTerrain(terrainSize, 512);
		InitHeightMap();
	}
	void Render(GLuint fbo) override//一定要画在这个fbo里
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.5f, 0.8f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(terrainShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, heightTex);
		glUniform1i(glGetUniformLocation(terrainShader, "heightMap"), 0);

		glUniform1f(
			glGetUniformLocation(terrainShader, "terrainSize"),
			terrainSize
		);

		glUniformMatrix4fv(
			glGetUniformLocation(terrainShader, "model"),
			1,
			GL_FALSE,
			glm::value_ptr(model)
		);

		glUniformMatrix4fv(
			glGetUniformLocation(terrainShader, "view"),
			1,
			GL_FALSE,
			glm::value_ptr(view)
		);

		glUniformMatrix4fv(
			glGetUniformLocation(terrainShader, "projection"),
			1,
			GL_FALSE,
			glm::value_ptr(projection)
		);

		glUniform1f(
			glGetUniformLocation(terrainShader, "time"),
			t
		);

		glUniform3f(
			glGetUniformLocation(terrainShader, "cameraPos"),
			cameraPos.x,
			cameraPos.y,
			cameraPos.z
		);


		DrawTerrain();
	}
	void Resize() override
	{
		auto bounds = GetBounds();
		printf("Root Bounds:%d %d %d %d\n", (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);

	}
};

class MyRootComponent :public Enola2::Component, public Enola2::EventListener
{
private:
	World v3d;
public:
	MyRootComponent()
	{
		AddChild(v3d);
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
		v3d.SetBounds({ 0,0,bounds.w,bounds.h });
	}
};