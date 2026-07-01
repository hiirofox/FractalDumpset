#define GLFW_INCLUDE_VULKAN
#include "Enola2Component.h"
#include "Enola2Event.h"

#include "ObjComponent.h"
#include "Shader.h"

class View3DWorld :public Enola2::Component
{
private:
	Shader shader;
	ObjComponent obj;
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

		obj.Reset();
		obj.Rotate(0.0, 0.0, 3.1415926 / 2);

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
MyRootComponent rootComponent;

//////////////////////////////////////
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	rootComponent.SetBounds({ 0, 0, (float)width, (float)height });
}
void KBCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	using namespace Enola2;
	KeyEvent e;
	e.key = key;
	if (action == GLFW_PRESS)
		e.state = KeyState::Down;
	else if (action == GLFW_RELEASE)
		e.state = KeyState::Up;
	else
		return;
	EventListener::DispatchKey(e);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}
void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	using namespace Enola2;
	MouseEvent e;
	e.x = static_cast<int>(xpos);
	e.y = static_cast<int>(ypos);
	e.state = MouseState::Move;
	EventListener::DispatchMouse(e);
}
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	using namespace Enola2;
	MouseEvent e;
	e.x = 0;
	e.y = 0;
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	e.x = (int)x;
	e.y = (int)y;
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		e.state = MouseState::LeftDown;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		e.state = MouseState::RightDown;
	else
		return;
	EventListener::DispatchMouse(e);
}
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	using namespace Enola2;
	MouseEvent e;
	e.state = MouseState::Wheel;
	e.wheel = (int)yoffset;
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	e.x = (int)x;
	e.y = (int)y;
	EventListener::DispatchMouse(e);
}
int main() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(800, 600, ".dumpset00", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	glfwSetKeyCallback(window, KBCallback);
	glfwSetCursorPosCallback(window, MouseCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetScrollCallback(window, ScrollCallback);

	rootComponent.SetBounds({ 0,0,800,600 });
	rootComponent.DoInit();

	// ---------------- Render loop ----------------
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();//处理鼠标键盘事件回调

		rootComponent.DoRender();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}