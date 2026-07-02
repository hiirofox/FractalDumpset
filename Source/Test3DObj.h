#pragma once


class BoundHighlighting : public Enola2::Component
{
private:
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint program = 0;

	GLuint CompileShader(GLenum type, const char* source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);
		return shader;
	}

public:
	void Init() override
	{
		const char* vertexSource = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            void main()
            {
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";
		const char* fragmentSource = R"(
            #version 330 core
            out vec4 FragColor;
            uniform vec4 color;
            void main()
            {
                FragColor = color;
            }
        )";

		GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource);
		GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

		program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);

		glDeleteShader(vertex);
		glDeleteShader(fragment);

		float vertices[] = {
			-1.0f, -1.0f,
			 1.0f, -1.0f,
			 1.0f,  1.0f,
			-1.0f,  1.0f
		};

		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void Render(GLuint fbo) override
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(program);
		glUniform4f(glGetUniformLocation(program, "color"), 0.0f, 1.0f, 0.0f, 1.0f);

		glLineWidth(4.0f);
		glBindVertexArray(vao);
		glDrawArrays(GL_LINE_LOOP, 0, 4);
		glBindVertexArray(0);
	}

	void Resize() override
	{
		auto b = GetBounds();
		printf("BoundHighlighting:%d %d %d %d\n",
			(int)b.x, (int)b.y, (int)b.w, (int)b.h);
	}

	~BoundHighlighting() override
	{
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
		glDeleteProgram(program);
	}
};

class View3DWorld :public Enola2::Component
{
private:
	Shader shader;
	ObjComponent obj;
	BoundHighlighting bhl;
	glm::vec3 cameraPos = glm::vec3(-8.0f, 6.0f, 0.0f);
	glm::vec3 cameraTarget = glm::vec3(0.0f, 3.0f, 0.0f);
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
		100.0f
	);

	GLuint gridVAO = 0;
	GLuint gridVBO = 0;
	int gridLineCount = 0;
	void InitGrid(float halfSize = 10.0f, int lines = 200)
	{
		std::vector<glm::vec3> vertices;
		std::vector<unsigned int> indices;

		float step = (halfSize * 2.0f) / lines;

		// 1. 生成“交叉点顶点”
		for (int z = 0; z <= lines; z++)
		{
			for (int x = 0; x <= lines; x++)
			{
				float px = -halfSize + x * step;
				float pz = -halfSize + z * step;

				vertices.push_back(glm::vec3(px, 0.0f, pz));
			}
		}

		// 2. 生成横向线索引
		for (int z = 0; z <= lines; z++)
		{
			for (int x = 0; x < lines; x++)
			{
				int i0 = z * (lines + 1) + x;
				int i1 = i0 + 1;

				indices.push_back(i0);
				indices.push_back(i1);
			}
		}

		// 3. 生成纵向线索引
		for (int z = 0; z < lines; z++)
		{
			for (int x = 0; x <= lines; x++)
			{
				int i0 = z * (lines + 1) + x;
				int i1 = i0 + (lines + 1);

				indices.push_back(i0);
				indices.push_back(i1);
			}
		}

		gridLineCount = (int)indices.size();

		GLuint EBO;

		glGenVertexArrays(1, &gridVAO);
		glGenBuffers(1, &gridVBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(gridVAO);

		// vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
		glBufferData(GL_ARRAY_BUFFER,
			vertices.size() * sizeof(glm::vec3),
			vertices.data(),
			GL_STATIC_DRAW);

		// index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			indices.size() * sizeof(unsigned int),
			indices.data(),
			GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

		glBindVertexArray(0);
	}


	GLuint dofFBO = 0;
	GLuint colorTex = 0;
	GLuint depthTex = 0;

	GLuint dofProgram = 0;
	GLuint quadVAO = 0;
	GLuint quadVBO = 0;

	int gWidth = 0;
	int gHeight = 0;
	GLuint CompileShader(GLenum type, const char* src)
	{
		GLuint s = glCreateShader(type);
		glShaderSource(s, 1, &src, nullptr);
		glCompileShader(s);
		return s;
	}
	GLuint CreateProgram(const char* vs, const char* fs)
	{
		GLuint v = CompileShader(GL_VERTEX_SHADER, vs);
		GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs);

		GLuint p = glCreateProgram();
		glAttachShader(p, v);
		glAttachShader(p, f);
		glLinkProgram(p);

		glDeleteShader(v);
		glDeleteShader(f);
		return p;
	}
	void DOF_Init(int width, int height)
	{
		gWidth = width;
		gHeight = height;

		// ===== 1. FBO =====
		glGenFramebuffers(1, &dofFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, dofFBO);

		// color texture
		glGenTextures(1, &colorTex);
		glBindTexture(GL_TEXTURE_2D, colorTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
			width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, colorTex, 0);

		// depth texture
		glGenTextures(1, &depthTex);
		glBindTexture(GL_TEXTURE_2D, depthTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
			width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, depthTex, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// ===== 2. fullscreen quad =====
		float quad[] = {
			-1, -1,  1, -1,  1,  1,
			-1, -1,  1,  1, -1,  1
		};

		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);

		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

		// ===== 3. shader =====
		const char* vs = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        out vec2 uv;
        void main() {
            uv = aPos * 0.5 + 0.5;
            gl_Position = vec4(aPos,0,1);
        }
    )";

		const char* fs = R"(
        #version 330 core
        in vec2 uv;
        out vec4 FragColor;

        uniform sampler2D uColor;
        uniform sampler2D uDepth;
        uniform float focal;
        uniform float strength;

        float getBlur(float d)
        {
            return clamp(abs(d - focal) * strength, 0.0, 1.0);
        }

        vec3 blur(vec2 uv, float r)
        {
            vec2 t = 1.0 / vec2(textureSize(uColor,0));
            vec3 c = vec3(0);

            c += texture(uColor, uv).rgb * 0.2;
            c += texture(uColor, uv + t*r*vec2(1,0)).rgb * 0.1;
            c += texture(uColor, uv - t*r*vec2(1,0)).rgb * 0.1;
            c += texture(uColor, uv + t*r*vec2(0,1)).rgb * 0.1;
            c += texture(uColor, uv - t*r*vec2(0,1)).rgb * 0.1;
            c += texture(uColor, uv + t*r*vec2(1,1)).rgb * 0.1;
            c += texture(uColor, uv - t*r*vec2(1,1)).rgb * 0.1;

            return c;
        }

        void main()
        {
            float d = texture(uDepth, uv).r;
            float b = getBlur(d);

            vec3 sharp = texture(uColor, uv).rgb;
            vec3 bl = blur(uv, b * 10.0);

            FragColor = vec4(mix(sharp, bl, b), 1.0);
        }
    )";
		dofProgram = CreateProgram(vs, fs);
	}
	void ApplyDepthOfField(GLuint outfbo)
	{
		// =========================
		// 1. 切换到默认屏幕 framebuffer
		// =========================
		// 说明：
		// dofFBO 里已经存了“场景图像 + 深度”
		// 这里要把结果画回屏幕（0 = default framebuffer）
		glBindFramebuffer(GL_FRAMEBUFFER, outfbo);

		// =========================
		// 2. 关闭深度测试
		// =========================
		// 说明：
		// 后处理是“画一张全屏图”，不需要深度测试
		// 如果不关，有可能导致 quad 被裁掉
		glDisable(GL_DEPTH_TEST);

		// =========================
		// 3. 使用 DoF 后处理 shader
		// =========================
		glUseProgram(dofProgram);

		// ======================================================
		// 4. 绑定“场景颜色纹理”（你看到的画面）
		// ======================================================
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTex);

		// uColor = 场景颜色图（必须是 FBO 渲染出来的）
		glUniform1i(glGetUniformLocation(dofProgram, "uColor"), 0);

		// ======================================================
		// 5. 绑定“深度纹理”（控制模糊程度的核心）
		// ======================================================
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthTex);

		// uDepth = 每个像素距离相机的深度
		glUniform1i(glGetUniformLocation(dofProgram, "uDepth"), 1);

		// ======================================================
		// 6. 焦平面（FOCAL PLANE）——最重要参数
		// ======================================================
		// 作用：
		// 哪个深度 = “完全清晰”
		//
		// 调它的效果：
		// - 变大 → 焦点往远处移动
		// - 变小 → 焦点往近处移动
		//
		// 典型范围：
		// depth 是 0~1（非线性）
		// 所以一般在 0.3 ~ 0.7 调
		glUniform1f(glGetUniformLocation(dofProgram, "focal"), 0.25f);

		// ======================================================
		// 7. 模糊强度（blur strength）
		// ======================================================
		// 作用：
		// 控制“离焦点越远，模糊增长速度”
		//
		// 调它的效果：
		// - 小（1~3） → 轻微虚化（真实镜头感）
		// - 中（4~8） → 明显景深
		// - 大（10+） → 夸张游戏风格
		//
		// 本质公式：
		// blur = abs(depth - focal) * strength
		glUniform1f(glGetUniformLocation(dofProgram, "strength"), 1.01f);

		// ======================================================
		// 8. 画一个“全屏矩形”
		// ======================================================
		// 说明：
		// quadVAO 是一个 [-1,1] 的屏幕覆盖三角形
		// fragment shader 会对每个像素做 DoF 计算
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	float t = 0;
public:
	View3DWorld()
	{
		AddChild(bhl);
	}
	void Init() override
	{
		shader.Init(
			"D:/Projects/c++/FractalDumpset/Resources/vertex.glsl",
			"D:/Projects/c++/FractalDumpset/Resources/fragment.glsl"
		);
		obj.Init("D:/Projects/c++/FractalDumpset/Resources/cat.obj");
		obj.Scale(1.0f);

		InitGrid(50.0f, 100);

		auto bounds = GetBounds();
		DOF_Init(bounds.w, bounds.h);
	}
	void Render(GLuint fbo) override
	{
		glBindFramebuffer(GL_FRAMEBUFFER, dofFBO);//在dofFBO上画

		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader.Use();
		glUniform3f(
			glGetUniformLocation(shader.ID, "cameraPos"),
			cameraPos.x, cameraPos.y, cameraPos.z
		);
		glUniform1f(
			glGetUniformLocation(shader.ID, "time"),
			t
		);

		// ====== 1. 先画 grid ======
		glm::mat4 identity = glm::mat4(1.0f);

		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"),
			1, GL_FALSE, glm::value_ptr(identity));

		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"),
			1, GL_FALSE, glm::value_ptr(view));

		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"),
			1, GL_FALSE, glm::value_ptr(projection));

		glUniform1i(glGetUniformLocation(shader.ID, "isGridDraw"), 1);
		glUniform1i(glGetUniformLocation(shader.ID, "cameraPos"), 1);

		glBindVertexArray(gridVAO);
		//glDrawArrays(GL_LINES, 0, gridLineCount);
		glBindVertexArray(gridVAO);
		glDrawElements(GL_LINES, gridLineCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		// ====== 2. 再画 obj ======
		obj.Reset();
		obj.Rotate(0.0, t * 4.0, 0.0);
		obj.Move(0, 1, 0);
		t += 0.001;

		glm::mat4 model = obj.GetModelMatrix();

		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"),
			1, GL_FALSE, glm::value_ptr(model));
		glUniform1i(glGetUniformLocation(shader.ID, "isGridDraw"), 0);

		obj.Draw(shader.ID);

		ApplyDepthOfField(fbo);//最后把dofFBO经过dof输出给组件fbo
	}
	void Resize() override
	{
		auto bounds = GetBounds();
		printf("3dWorld Bounds:%d %d %d %d\n", (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);
		bhl.SetBounds({ 0,0,bounds.w,bounds.h });
	}
};