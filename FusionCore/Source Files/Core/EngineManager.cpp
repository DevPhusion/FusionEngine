#include "../../Header Files/Core/EngineManager.h"
#include "../../Header Files/Core/ObjectManager.h"
#include "../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "../../Header Files/Core/Editor/Windows/Viewport.h"
#include "../../Header Files/Core/SceneManager.h"

void EngineManager::Setup(GLFWwindow* window) {
	mainThreadId = std::this_thread::get_id();

	int windowWidth, windowHeight;
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
	this->windowWidth = (float)windowWidth;
	this->windowHeight = (float)windowHeight;
	this->aspectRatio = this->windowWidth / this->windowHeight;
	frameCount = 0;

	this->Window = window;
	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
}

void EngineManager::ProcessEngine(float delta) {
	ProcessPendingMainThreadTasks();

	frameCount++;
	time += delta;

	if (time >= 1) {
		fps = frameCount / time;
		frameCount = 0;
		time = 0;
	}
}

void EngineManager::ProcessPendingMainThreadTasks() {
	std::queue<std::function<void()>> tasksToRun;
	{
		std::lock_guard<std::mutex> lock(mainThreadQueueMutex);
		std::swap(tasksToRun, mainThreadTaskQueue);
	}

	while (!tasksToRun.empty()) {
		tasksToRun.front()();
		tasksToRun.pop();
	}
}

void EngineManager::SceneChangeEvent() {
	if (EnginePhysicsMode == PhysicsMode::Stop) {
		SceneManager::getInstance().MarkActiveDirty();
	}
}

void EngineManager::EngineChangeEvent() {
	if (EnginePhysicsMode == PhysicsMode::Stop) {
		FileManager::getInstance().isProjectSaved = false;
	}
}

void EngineManager::SetGameResolution(float width, float height) {
	if (width <= 0.0f || height <= 0.0f) return;

	resolutionWidth = width;
	resolutionHeight = height;
	gameAspectRatio = resolutionWidth / resolutionHeight;

	Viewport* gameViewport = EditorManager::getInstance().gameViewport;
	if (gameViewport) {
		gameViewport->Resize((int)resolutionWidth, (int)resolutionHeight);
	}
}

ViewportRect EngineManager::GetPlayerViewportRect() const {
	ViewportRect rect;
	if (windowWidth <= 0 || windowHeight <= 0 || gameAspectRatio <= 0) {
		rect.width = (int)windowWidth;
		rect.height = (int)windowHeight;
		return rect;
	}

	float windowAspect = windowWidth / windowHeight;

	if (windowAspect > gameAspectRatio) {
		rect.height = (int)windowHeight;
		rect.width = (int)(windowHeight * gameAspectRatio);
	}
	else {
		rect.width = (int)windowWidth;
		rect.height = (int)(windowWidth / gameAspectRatio);
	}

	rect.x = (int)((windowWidth - rect.width) * 0.5f);
	rect.y = (int)((windowHeight - rect.height) * 0.5f);
	return rect;
}

void EngineManager::SerializeEngineSettings(BinaryWriter& w) {
	Settings& s = EngineSettings;

	w.WriteString(EngineSettings.mainScenePath);

	w.Write(resolutionWidth);
	w.Write(resolutionHeight);
	
	w.Write(s.backgroundColor);
	w.Write(s.drawBackgroundGrid);
	w.Write((int)s.broadPhaseMode);
	w.Write(s.drawObjectWireframe);
	w.Write(s.drawBroadPhaseBounds);
	w.Write(s.drawCollisionShapes);
	w.Write(s.drawCollisionNormals);
	w.Write(s.drawContactPoints);
	w.Write(s.drawSoftBodyPointMasses);
	w.Write(s.drawSoftBodySprings);
	w.Write(s.drawVirtualSoftBodyProxies);
	w.Write(s.drawFluidsAsParticles);
	w.Write(s.drawFluidsVelocityField);
	w.Write(static_cast<uint32_t>(s.fluidHeatmapMode));
}

void EngineManager::DeserializeEngineSettings(BinaryReader& r) {
	Settings& s = EngineSettings;

	EngineSettings.mainScenePath = r.ReadString();

	float resWidth = r.Read<float>();
	float resHeight = r.Read<float>();
	SetGameResolution(resWidth, resHeight);

	s.backgroundColor = r.Read<glm::vec4>();
	s.drawBackgroundGrid = r.Read<bool>();
	s.broadPhaseMode = static_cast<BroadPhaseMode>(r.Read<int>());
	s.drawObjectWireframe = r.Read<bool>();
	s.drawBroadPhaseBounds = r.Read<bool>();
	s.drawCollisionShapes = r.Read<bool>();
	s.drawCollisionNormals = r.Read<bool>();
	s.drawContactPoints = r.Read<bool>();
	s.drawSoftBodyPointMasses = r.Read<bool>();
	s.drawSoftBodySprings = r.Read<bool>();
	s.drawVirtualSoftBodyProxies = r.Read<bool>();
	s.drawFluidsAsParticles = r.Read<bool>();
	s.drawFluidsVelocityField = r.Read<bool>();
	s.fluidHeatmapMode = static_cast<FluidHeatmapMode>(r.Read<uint32_t>());
}

void EngineManager::SwitchInteractMode(InteractMode mode) {
	EngineInteractMode = mode;

	for (const auto& [id, func] : InteractModeChangedEvents) {
		func();
	}
}

void EngineManager::SwitchPhysicsMode(PhysicsMode mode) {
	EnginePrevPhysicsMode = EnginePhysicsMode;
	EnginePhysicsMode = mode;

	for (const auto& [id, func] : PhysicsModeChangedEvents) {
		func();
	}
}

int EngineManager::AddInteractModeChangedEvent(std::function<void()> func) {
	CurrentInteractModeChangedID += 1;
	InteractModeChangedEvents[CurrentInteractModeChangedID] = func;
	return CurrentInteractModeChangedID;
}

int EngineManager::AddPhysicsModeChangedEvent(std::function<void()> func) {
	CurrentPhysicsModeChangedID += 1;
	PhysicsModeChangedEvents[CurrentPhysicsModeChangedID] = func;
	return CurrentPhysicsModeChangedID;
}

void EngineManager::RemoveInteractModeChangedEvent(int ID) {
	InteractModeChangedEvents.erase(ID);
}

void EngineManager::RemovePhysicsModeChangedEvent(int ID) {
	PhysicsModeChangedEvents.erase(ID);
}

void EngineManager::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
	if (width == 0 || height == 0) return;

	EngineManager& eng = EngineManager::getInstance();
	eng.windowWidth = (float)width;
	eng.windowHeight = (float)height;
	eng.aspectRatio = eng.windowWidth / eng.windowHeight;

	glViewport(0, 0, width, height);
}

void EngineManager::WindowCloseCallback(GLFWwindow* window) {
	FileManager& FM = FileManager::getInstance();
	EngineManager& eng = EngineManager::getInstance();
	if (!ProjectLauncher::getInstance().HasEnteredProject()) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}
	if (!FM.isProjectSaved || SceneManager::getInstance().AnySceneDirty()) {
		glfwSetWindowShouldClose(window, GLFW_FALSE);
		eng.pendingClose = true;
	}
}