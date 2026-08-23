#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<iostream>
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include "../FusionCore/Header Files/Core/InputManager.h"
#include "../FusionCore/Header Files/Core/Rendering/Renderer.h"
#include "../FusionCore/Header Files/Core/Physics/PhysicsEngine.h"
#include "../FusionCore/Header Files/Core/EngineManager.h"
#include "../FusionCore/Header Files/Core/Editor/EditorManager.h"
#include "../FusionCore/Header Files/Core/ObjectManager.h"
#include "../FusionCore/Header Files/Core/Files/ProjectLauncher.h"
#include "../FusionCore/Header Files/Core/Files/FileManager.h"
#include "../FusionCore/Header Files/Core/Rendering/Shader.h"
#include "../FusionCore/Header Files/Core/Scripting/PyBindings.h"
#include "../FusionCore/Header Files/Core/Scripting/ScriptManager.h"
#include "../FusionCore/Header Files/Core/SceneManager.h"
#include "../FusionCore/Header Files/Core/Editor/HeadlessMonitor.h"


PYBIND11_EMBEDDED_MODULE(fusion, m) {
	RegisterEngineBindings(m);
}

void SetWorkingDirectoryToExePath() {
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);

	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
	SetCurrentDirectoryA(exeDir.string().c_str());
}

namespace {
	struct LaunchArgs {
		std::string projectPath;
	};

	LaunchArgs ParseLaunchArgs(int argc, char* argv[]) {
		LaunchArgs args;
		for (int i = 1; i < argc; i++) {
			std::string arg = argv[i];
			if (args.projectPath.empty())
				args.projectPath = arg;
		}
		return args;
	}
}

int main(int argc, char* argv[]) {
	SetWorkingDirectoryToExePath();

	LaunchArgs launchArgs = ParseLaunchArgs(argc, argv);

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	const int launcherWidth = 900;
	const int launcherHeight = 600;

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(launcherWidth, launcherHeight, "Fusion Engine - Projects", NULL, NULL);
	glfwMakeContextCurrent(window);
	gladLoadGL();

	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	if (mode) {
		int posX = (mode->width - launcherWidth) / 2;
		int posY = (mode->height - launcherHeight) / 2;
		glfwSetWindowPos(window, posX, posY);
	}
	glfwShowWindow(window);

	GLFWimage images[1];
	images[0].pixels = stbi_load("Resources/Images/engineIcon.png", &images[0].width, &images[0].height, 0, 4);
	if (images[0].pixels) {
		glfwSetWindowIcon(window, 1, images);
		stbi_image_free(images[0].pixels);
	}

	EngineManager::getInstance().isPlayer = false;

	Renderer::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	EditorManager::getInstance().Setup(window);

	InputManager::getInstance().Setup(window);
	EngineManager::getInstance().Setup(window);
	PhysicsEngine::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	Camera::getInstance().Setup();

	Renderer::getInstance().SetupGrid();

	ProjectLauncher::getInstance().Setup(window);
	glfwPollEvents();

	if (!launchArgs.projectPath.empty()) {
		if (!ProjectLauncher::getInstance().OpenProjectFile(launchArgs.projectPath))
			std::cerr << "Failed to load project from launch args: " << launchArgs.projectPath << std::endl;
	}

	float prev_t = glfwGetTime();
	float physicsAccumulator = 0.0f;
	const float PHYSICS_STEP = 1.0f / 60.0f;
	bool enteredEditor = false;
	bool enteredHeadlessMonitor = false;
	double headlessMonitorLastDraw = 0.0;

	bool prevWantRealtime = false;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		if (!ProjectLauncher::getInstance().HasEnteredProject()) {
			glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			ProjectLauncher::getInstance().ProcessLauncher();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(window);
			continue;
		}

		if (EngineManager::getInstance().isHeadless) {
			bool wantRealtime = EngineManager::getInstance().liveTrainingRenderActive.load();

			if (!enteredHeadlessMonitor) {
				enteredHeadlessMonitor = true;
				glfwSwapInterval(wantRealtime ? 1 : 0);
				headlessMonitorLastDraw = 0.0;
			}
			else if (wantRealtime != prevWantRealtime) {
				glfwSwapInterval(wantRealtime ? 1 : 0);   
			}
			prevWantRealtime = wantRealtime;

			const double MONITOR_REFRESH_INTERVAL = wantRealtime ? 0.0 : (1.0 / 30.0);
			double drawNow = glfwGetTime();
			if (drawNow - headlessMonitorLastDraw >= MONITOR_REFRESH_INTERVAL) {
				headlessMonitorLastDraw = drawNow;

				glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);

				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();

				HeadlessMonitor::getInstance().ProcessMonitorWindow();

				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

				glfwSwapBuffers(window);
			}

			continue;
		}
		else if (enteredHeadlessMonitor) {
			enteredHeadlessMonitor = false;
			glfwSwapInterval(1);
		}

		if (!enteredEditor) {
			enteredEditor = true;
			glfwSetWindowTitle(window, "Fusion Engine");
			glfwMaximizeWindow(window);
			glfwPollEvents();
			prev_t = glfwGetTime();
			physicsAccumulator = 0.0f;
		}

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

		SceneManager::getInstance().ProcessPendingSceneLoad();
		ScriptManager::getInstance().Update();
		EngineManager::getInstance().ProcessEngine(delta);

		Viewport* gameViewport = EditorManager::getInstance().gameViewport;
		glm::vec4& bg = EngineManager::getInstance().EngineSettings.backgroundColor;

		gameViewport->BeginRenderGame();
		glad_glClearColor(bg.r, bg.g, bg.b, bg.a);
		glClear(GL_COLOR_BUFFER_BIT);
		Renderer::getInstance().Draw();
		gameViewport->EndRenderGame();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, (int)EngineManager::getInstance().windowWidth, (int)EngineManager::getInstance().windowHeight);
		glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		EditorManager::getInstance().ProcessEditor();

		InputManager::getInstance().ClearFrameState();

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	ImPlot::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}