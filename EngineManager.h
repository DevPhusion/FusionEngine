#pragma once
#include "InputManager.h"
#include <functional>

struct Settings {
	glm::vec4 backgroundColor = glm::vec4(0.235f, 0.239f, 0.216f, 1.0f);
	bool drawBackgroundGrid = true;
	bool drawBroadPhaseBounds = false;
	bool colorCollisions = false;
	bool drawCollisionNormals = false;
	bool drawContactPoints = false;
	bool drawSoftBodyPointMasses = false;
	bool drawSoftBodySprings = false;
	bool drawVirtualSoftBodyProxies = false;

	// Everything except drawBackgroundGrid
	bool AnyDebugGizmoEnabled() const {
		return drawBroadPhaseBounds || drawCollisionNormals || drawContactPoints ||
			drawSoftBodyPointMasses || drawSoftBodySprings || drawVirtualSoftBodyProxies;
	}
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
		Simulate
	};

	InteractMode EngineInteractMode = EditorSelect;
	PhysicsMode EnginePhysicsMode = Pause;

	Settings EngineSettings;

	float fps;
	float windowWidth;
	float windowHeight;
	float aspectRatio;
	std::unordered_map<int, std::function<void()>> InteractModeChangedEvents;
	std::unordered_map<int, std::function<void()>> PhysicsModeChangedEvents;

	void Setup(GLFWwindow* window);
	void ProcessEngine(float delta);
	void SwitchInteractMode(InteractMode mode);
	void SwitchPhysicsMode(PhysicsMode mode);
	int AddInteractModeChangedEvent(std::function<void()> func);
	int AddPhysicsModeChangedEvent(std::function<void()> func);
	void RemovePhysicsModeChangedEvent(int ID);
	void RemoveInteractModeChangedEvent(int ID);
private:
	int CurrentInteractModeChangedID = -1;
	int CurrentPhysicsModeChangedID = -1;
	float time;
	float frameCount;
	EngineManager() = default;

};

