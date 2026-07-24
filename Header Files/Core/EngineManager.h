#pragma once
#include "InputManager.h"
#include "Files/ProjectLauncher.h"
#include <fstream>
#include <stdexcept>
#include <memory>
#include <functional>

class Constraint;

enum class FluidHeatmapMode {
	None,
	Velocity,
	Density
};

struct Settings {
	glm::vec4 backgroundColor = glm::vec4(0.235f, 0.239f, 0.216f, 1.0f);
	bool drawBackgroundGrid = true;
	bool drawObjectWireframe = false;
	bool drawBroadPhaseBounds = false;
	bool colorCollisions = false;
	bool drawCollisionNormals = false;
	bool drawContactPoints = false;
	bool drawSoftBodyPointMasses = false;
	bool drawSoftBodySprings = false;
	bool drawVirtualSoftBodyProxies = false;
	bool drawFluidsAsParticles = false;
	bool drawFluidsVelocityField = false;
	FluidHeatmapMode fluidHeatmapMode = FluidHeatmapMode::None;

	bool AnyDebugGizmoEnabled() const {
		return drawObjectWireframe || drawBroadPhaseBounds || drawCollisionNormals || drawContactPoints ||
			drawSoftBodyPointMasses || drawSoftBodySprings || drawVirtualSoftBodyProxies
			|| drawFluidsAsParticles || drawFluidsVelocityField || fluidHeatmapMode != FluidHeatmapMode::None;
	}
};

struct EngineState {
	std::vector<std::unique_ptr<Object>> Objects = {};
	std::vector<std::shared_ptr<Constraint>> Constraints = {};
};

class EngineManager
{
public:
	EngineManager(const EngineManager&) = delete;
	void operator=(const EngineManager&) = delete;

	static EngineManager& getInstance() {
		static EngineManager instance;
		return instance;
	}

	enum InteractMode {
		AddVertex,
		EditorSelect,
	};

	enum PhysicsMode {
		Pause,
		Stop,
		Simulate
	};

	InteractMode EngineInteractMode = EditorSelect;
	PhysicsMode EnginePhysicsMode = Stop;
	bool pendingClose = false;

	Settings EngineSettings;
	
	EngineState SavedState;
	std::vector<std::unique_ptr<Object>> cachedSaveObjects;

	GLFWwindow* Window = nullptr;

	float fps;
	float windowWidth;
	float windowHeight;
	float aspectRatio;
	std::unordered_map<int, std::function<void()>> InteractModeChangedEvents;
	std::unordered_map<int, std::function<void()>> PhysicsModeChangedEvents;

	void Setup(GLFWwindow* window);
	void ProcessEngine(float delta);
	void SaveEngineState();
	void LoadEngineState();
	void EngineChangeEvent();
	void SwitchInteractMode(InteractMode mode);
	void SwitchPhysicsMode(PhysicsMode mode);
	int AddInteractModeChangedEvent(std::function<void()> func);
	int AddPhysicsModeChangedEvent(std::function<void()> func);
	void RemovePhysicsModeChangedEvent(int ID);
	void RemoveInteractModeChangedEvent(int ID);
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void WindowCloseCallback(GLFWwindow* window);
private:
	int CurrentInteractModeChangedID = -1;
	int CurrentPhysicsModeChangedID = -1;
	float time;
	float frameCount;
	EngineManager() = default;

};

