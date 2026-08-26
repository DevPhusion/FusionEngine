#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<iostream>
#include<fstream>
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include "../FusionCore/Header Files/Core/InputManager.h"
#include "../FusionCore/Header Files/Core/Rendering/Renderer.h"
#include "../FusionCore/Header Files/Core/Physics/PhysicsEngine.h"
#include "../FusionCore/Header Files/Core/EngineManager.h"
#include "../FusionCore/Header Files/Core/ObjectManager.h"
#include "../FusionCore/Header Files/Core/Files/ProjectLauncher.h"
#include "../FusionCore/Header Files/Core/Files/Export/ExportPackageReader.h"
#include "../FusionCore/Header Files/Core/Rendering/Shader.h"
#include "../FusionCore/Header Files/Core/Scripting/PyBindings.h"
#include "../FusionCore/Header Files/Core/Scripting/ScriptManager.h"
#include "../FusionCore/Header Files/Core/SceneManager.h" 

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

std::filesystem::path GetExeDirectory() {
	char exePathBuf[MAX_PATH];
	GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
	return std::filesystem::path(exePathBuf).parent_path();
}

std::string ResolveLooseProjectFilePath(const std::filesystem::path& exeDir) {
	std::error_code ec;

	char exePathBuf[MAX_PATH];
	GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
	std::filesystem::path exeStem = std::filesystem::path(exePathBuf).stem();

	std::filesystem::path candidate = exeDir / (exeStem.string() + ".fusion");
	if (std::filesystem::exists(candidate, ec)) return candidate.string();

	for (auto& entry : std::filesystem::directory_iterator(exeDir, ec)) {
		if (ec) break;
		if (entry.is_regular_file() && entry.path().extension() == ".fusion") {
			return entry.path().string();
		}
	}

	return "";
}

std::string ReadExportedGameName(const std::filesystem::path& exeDir) {
	std::filesystem::path infoPath = exeDir / "export_info.json";

	std::error_code ec;
	if (!std::filesystem::exists(infoPath, ec)) return "";

	std::ifstream in(infoPath);
	if (!in.is_open()) return "";

	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

	const std::string key = "\"name\"";
	size_t keyPos = content.find(key);
	if (keyPos == std::string::npos) return "";

	size_t colonPos = content.find(':', keyPos);
	if (colonPos == std::string::npos) return "";

	size_t firstQuote = content.find('"', colonPos);
	if (firstQuote == std::string::npos) return "";

	size_t secondQuote = content.find('"', firstQuote + 1);
	if (secondQuote == std::string::npos) return "";

	return content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

bool LoadProjectForPlayer(const std::filesystem::path& exeDir, const std::string& explicitPath) {
	if (!explicitPath.empty()) {
		return ProjectLauncher::getInstance().OpenProjectFile(explicitPath);
	}

	std::filesystem::path packPath = exeDir / "data.pack";
	std::error_code ec;

	if (std::filesystem::exists(packPath, ec) && ExportPackageReader::getInstance().Load(packPath)) {
		const std::vector<uint8_t>* fusionBytes = ExportPackageReader::getInstance().Get("__project__");
		if (!fusionBytes) {
			FatalError("data.pack is missing the project data entry.");
			return false;
		}

		FileManager::getInstance().currentProjectDirectory = exeDir.string();
		FileManager::getInstance().currentProjectFile = (exeDir / "project.fusion").string();
		FileManager::getInstance().LoadProjectFromMemory(*fusionBytes);
		FileManager::getInstance().SetupResourcesFolder(); 
		return true;
	}

	std::string loosePath = ResolveLooseProjectFilePath(exeDir);
	if (loosePath.empty()) return false;
	return ProjectLauncher::getInstance().OpenProjectFile(loosePath);
}

int main(int argc, char* argv[]) {
	SetWorkingDirectoryToExePath();

	std::filesystem::path exeDir = GetExeDirectory();
	std::string explicitPath = (argc > 1) ? argv[1] : "";
	std::string gameName = ReadExportedGameName(exeDir);
	std::string windowTitle = gameName.empty() ? "Fusion Player" : gameName;

	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	const int windowWidth = 1280;
	const int windowHeight = 720;

	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(), NULL, NULL);
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

	EngineManager::getInstance().isPlayer = true;
	Renderer::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	InputManager::getInstance().Setup(window);
	EngineManager::getInstance().Setup(window);
	PhysicsEngine::getInstance().Setup(&ObjectManager::getInstance().allObjects);
	Camera::getInstance().Setup();
	Renderer::getInstance().SetupGrid();

	if (!LoadProjectForPlayer(exeDir, explicitPath)) {
		FatalError("No project data found next to the executable.");
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	glfwShowWindow(window);
	glfwMaximizeWindow(window);
	glfwPollEvents();

	float prev_t = glfwGetTime();
	float physicsAccumulator = 0.0f;
	const float PHYSICS_STEP = 1.0f / 60.0f;

	bool enteredSimulateMode = false;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ScriptManager::getInstance().Update();

		if (!enteredSimulateMode && !ScriptManager::getInstance().IsBusy()) {
			enteredSimulateMode = true;
			EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
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

		EngineManager::getInstance().ProcessEngine(delta);

		glm::vec4& bg = EngineManager::getInstance().EngineSettings.backgroundColor;

		ViewportRect vp = EngineManager::getInstance().GetPlayerViewportRect();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, (int)EngineManager::getInstance().windowWidth, (int)EngineManager::getInstance().windowHeight);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_SCISSOR_TEST);
		glViewport(vp.x, vp.y, vp.width, vp.height);
		glScissor(vp.x, vp.y, vp.width, vp.height);
		glClearColor(bg.r, bg.g, bg.b, bg.a);
		glClear(GL_COLOR_BUFFER_BIT);

		Renderer::getInstance().Draw();

		glDisable(GL_SCISSOR_TEST);

		InputManager::getInstance().ClearFrameState();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}