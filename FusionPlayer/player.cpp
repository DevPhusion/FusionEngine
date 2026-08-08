#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<iostream>
#include <windows.h>
#include <filesystem>
#include "../FusionCore/Header Files/Core/InputManager.h"
#include "../FusionCore/Header Files/Core/Rendering/Renderer.h"
#include "../FusionCore/Header Files/Core/Physics/PhysicsEngine.h"
#include "../FusionCore/Header Files/Core/EngineManager.h"
#include "../FusionCore/Header Files/Core/ObjectManager.h"
#include "../FusionCore/Header Files/Core/Files/ProjectLauncher.h"
#include "../FusionCore/Header Files/Core/Rendering/Shader.h"
#include "../FusionCore/Header Files/Core/Scripting/PyBindings.h"

PYBIND11_EMBEDDED_MODULE(fusion, m) {
	RegisterEngineBindings(m);
}

void SetWorkingDirectoryToExePath() {
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);

	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
	SetCurrentDirectoryA(exeDir.string().c_str());
}

void FatalError(const std::string& message) {
	std::cerr << "FusionPlayer: " << message << std::endl;
	MessageBoxA(NULL, message.c_str(), "Fusion Player - Error", MB_OK | MB_ICONERROR);
}

std::string ResolveProjectFilePath(int argc, char* argv[]) {
	if (argc > 1) return argv[1];

	char exePathBuf[MAX_PATH];
	GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
	std::filesystem::path exePath(exePathBuf);
	std::filesystem::path exeDir = exePath.parent_path();

	std::error_code ec;

	std::filesystem::path candidate = exeDir / (exePath.stem().string() + ".fusion");
	if (std::filesystem::exists(candidate, ec)) return candidate.string();

	for (auto& entry : std::filesystem::directory_iterator(exeDir, ec)) {
		if (ec) break;
		if (entry.is_regular_file() && entry.path().extension() == ".fusion") {
			return entry.path().string();
		}
	}

	return "";
}

int main(int argc, char* argv[]) {
	SetWorkingDirectoryToExePath();

	std::string launchPath = ResolveProjectFilePath(argc, argv);
	if (launchPath.empty()) {
		FatalError("No .fusion project file found next to the executable, and none was specified.");
		return 1;
	}

	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	const int windowWidth = 1280;
	const int windowHeight = 720;

	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Fusion Player", NULL, NULL);
	if (!window) {
		FatalError("Failed to create window. Your GPU or drivers may not support OpenGL 3.3.");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL()) {
		FatalError("Failed to initialize OpenGL (gladLoadGL failed).");
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	{
		const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		if (mode) {
			int posX = (mode->width - windowWidth) / 2;
			int posY = (mode->height - windowHeight) / 2;
			glfwSetWindowPos(window, posX, posY);
		}
	}

	GLFWimage images[1];
	images[0].pixels = stbi_load("Resources/Images/engineIcon.png", &images[0].width, &images[0].height, 0, 4);
	if (images[0].pixels) {
		glfwSetWindowIcon(window, 1, images);
		stbi_image_free(images[0].pixels);
	}

	Renderer::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	InputManager::getInstance().Setup(window);
	EngineManager::getInstance().Setup(window);
	PhysicsEngine::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	Camera::getInstance().Setup();
	Renderer::getInstance().SetupGrid();

	if (!ProjectLauncher::getInstance().OpenProjectFile(launchPath)) {
		FatalError("Failed to load project: " + launchPath);
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);

	glfwShowWindow(window);
	glfwPollEvents();

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

		InputManager::getInstance().DispatchFrameEvents();
		Camera::getInstance().ProcessCamera(delta);

		while (physicsAccumulator >= PHYSICS_STEP) {
			PhysicsEngine::getInstance().ProcessPhysics(PHYSICS_STEP);
			physicsAccumulator -= PHYSICS_STEP;
		}

		EngineManager::getInstance().ProcessEngine(delta);

		glm::vec4& bg = EngineManager::getInstance().EngineSettings.backgroundColor;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, (int)EngineManager::getInstance().windowWidth, (int)EngineManager::getInstance().windowHeight);
		glClearColor(bg.r, bg.g, bg.b, bg.a);
		glClear(GL_COLOR_BUFFER_BIT);

		Renderer::getInstance().Draw();

		InputManager::getInstance().ClearFrameState();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}