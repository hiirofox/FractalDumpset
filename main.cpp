#define GLFW_INCLUDE_VULKAN
#include "ObjComponent.h"
#include "Shader.h"


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, ".dumpset00", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    // ---------------- Shader ----------------
    Shader shader(
        "D:/Projects/c++/FractalDumpset/Resources/vertex.glsl",
        "D:/Projects/c++/FractalDumpset/Resources/fragment.glsl"
    );

    // ---------------- Load OBJ ----------------
    ObjComponent obj("D:/Projects/c++/FractalDumpset/Resources/fd.obj");

    obj.Scale(1.0f);

    // ---------------- Camera（关键新增） ----------------
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

    // ---------------- Render loop ----------------
    while (!glfwWindowShouldClose(window)) {

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.Use();

        obj.Rotate(0.0f, 0.0001f, 0.0001f);

        glm::mat4 model = obj.GetModelMatrix(); // ❗需要你加 getter

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"),
            1, GL_FALSE, glm::value_ptr(model));

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"),
            1, GL_FALSE, glm::value_ptr(view));

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"),
            1, GL_FALSE, glm::value_ptr(projection));

        obj.Draw(shader.ID);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}