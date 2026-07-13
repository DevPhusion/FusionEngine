#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<iostream>
#include <windows.h>
#include <filesystem>
#include "Polygon.h"
#include "InputManager.h"
#include "Renderer.h"
#include "PhysicsEngine.h"
#include "EngineManager.h"
#include "EditorManager.h"
#include "ObjectManager.h"
#include "Shader.h"
#include "VertexPoint.h"
#include "MouseDrag.h"

void cursorPressedCallback(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		ObjectManager::getInstance().AddPolygonVertex();
	}
}

void keyPressed(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_R) {
		if (action == GLFW_PRESS) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else if (action == GLFW_RELEASE) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}
}

void SetWorkingDirectoryToExePath() {
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);

	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
	SetCurrentDirectoryA(exeDir.string().c_str());
}

int main(int argc, char* argv[]) {
	SetWorkingDirectoryToExePath();
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	/*
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

	GLFWwindow* window = glfwCreateWindow(
		mode->width, mode->height, "Fusion physics", monitor, NULL
	);
	*/

	GLFWwindow* window = glfwCreateWindow(1920, 1080, "Fusion physics", NULL, NULL);
	glfwMakeContextCurrent(window);
	gladLoadGL();

	GLFWimage images[1];
	images[0].pixels = stbi_load("engineIcon.png", &images[0].width, &images[0].height, 0, 4); // Forces RGBA channels

	if (images[0].pixels) {
		glfwSetWindowIcon(window, 1, images);
		stbi_image_free(images[0].pixels); // Clean up memory safely after setting
	}
	
	Renderer::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	EditorManager::getInstance().Setup(window);

	InputManager::getInstance().Setup(window);
	InputManager::getInstance().SetKeyButtonCallback(keyPressed, 999);
	InputManager::getInstance().SetMouseButtonCallback(cursorPressedCallback, 999);

	EngineManager::getInstance().Setup(window);
	PhysicsEngine::getInstance().Setup(&ObjectManager::getInstance().allObjects);

	Camera::getInstance().Setup();
	Renderer::getInstance().SetupGrid();

	if (argc > 1) {
		std::string launchPath = argv[1];
		try {
			EngineManager::getInstance().LoadProjectFromFile(launchPath);
		}
		catch (const std::exception& e) {
			std::cerr << "Failed to load project from launch args: " << e.what() << std::endl;
		}
	}

	float prev_t = glfwGetTime(); 
	float physicsAccumulator = 0.0f;
	const float PHYSICS_STEP = 1.0f / 60.0f;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float now = glfwGetTime();
		float delta = now - prev_t;
		prev_t = now; 

		if (delta > 0.1f) delta = 0.1f;
		physicsAccumulator += delta;

		Camera::getInstance().ProcessCamera(delta);

		while (physicsAccumulator >= PHYSICS_STEP) {
			PhysicsEngine::getInstance().ProcessPhysics(PHYSICS_STEP);
			physicsAccumulator -= PHYSICS_STEP;
		}

		EngineManager::getInstance().ProcessEngine(delta);
		ObjectManager::getInstance().ProcessObjects(delta);

		glm::vec4& bg = EngineManager::getInstance().EngineSettings.backgroundColor;
		glad_glClearColor(bg.r, bg.g, bg.b, bg.a);
		glClear(GL_COLOR_BUFFER_BIT);

		Renderer::getInstance().Draw();
		EditorManager::getInstance().ProcessEditor();

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}