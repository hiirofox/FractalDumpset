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

	void Render() override
	{
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(program);
		glUniform4f(glGetUniformLocation(program, "color"), 1.0f, 0.85f, 0.15f, 1.0f);

		glLineWidth(2.0f);
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
	glm::mat4 view = glm::lookAt(
		glm::vec3(0.0f, 5.0f, 0.0f),  // 相机在“上方”
		glm::vec3(0.0f, 0.0f, 0.0f),  // 看向原点
		glm::vec3(0.0f, 0.0f, -1.0f)  // “上方向”
	);
	glm::mat4 projection = glm::perspective(
		glm::radians(45.0f),
		800.0f / 600.0f,
		0.1f,
		100.0f
	);
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
		obj.Init("D:/Projects/c++/FractalDumpset/Resources/fd.obj");
		obj.Scale(1.0f);
	}
	void Render() override
	{
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader.Use();

		//obj.Reset();
		//obj.Rotate(0.0, 0.0, 3.1415926 / 2);
		obj.Rotate(0.0, 0.01, 0.01);

		glm::mat4 model = obj.GetModelMatrix(); // ❗需要你加 getter

		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"),
			1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"),
			1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"),
			1, GL_FALSE, glm::value_ptr(projection));

		obj.Draw(shader.ID);
	}
	void Resize() override
	{
		auto bounds = GetBounds();
		printf("3dWorld Bounds:%d %d %d %d\n", (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);
		bhl.SetBounds({ 0,0,bounds.w,bounds.h });
	}
};
class MyRootComponent :public Enola2::Component, public Enola2::EventListener
{
private:
	View3DWorld v3d;
public:
	MyRootComponent()
	{
		AddChild(v3d);
	}
	void Init() override
	{

	}
	void Render() override
	{

	}
	void Resize() override
	{
		auto bounds = GetBounds();
		printf("Root Bounds:%d %d %d %d\n", (int)bounds.x, (int)bounds.y, (int)bounds.w, (int)bounds.h);
		v3d.SetBounds({ 32,32,bounds.w - 64,bounds.h - 64 });
	}
};