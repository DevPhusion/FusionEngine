#pragma once
#include "InputManager.h"
#include "Files/ProjectLauncher.h"
#include <fstream>
#include <stdexcept>
#include <memory>
#include <functional>
#include <mutex>
#include <future>
#include <queue>
#include <thread>

class Constraint;

enum class FluidHeatmapMode {
	None,
	Velocity,
	Density
};

struct Settings {
	glm::vec4 backgroundColor = glm::vec4(0.235f, 0.239f, 0.216f, 1.0f);
	glm::vec2 gameResolution = glm::vec2(1920, 1080);
	bool drawBackgroundGrid = true;
	std::string mainScenePath = "";

	// Debug
	bool drawObjectWireframe = false;
	bool drawBroadPhaseBounds = false;
	bool drawCollisionShapes = false;
	bool drawCollisionNormals = false;
	bool drawContactPoints = false;
	bool drawSoftBodyPointMasses = false;
	bool drawSoftBodySprings = false;
	bool drawVirtualSoftBodyProxies = false;
	bool drawFluidsAsParticles = false;
	bool drawFluidsVelocityField = false;
	FluidHeatmapMode fluidHeatmapMode = FluidHeatmapMode::None;

	bool AnyDebugGizmoEnabled() const {
		return drawObjectWireframe || drawBroadPhaseBounds || drawCollisionShapes || drawCollisionNormals || drawContactPoints ||
			drawSoftBodyPointMasses || drawSoftBodySprings || drawVirtualSoftBodyProxies
			|| drawFluidsAsParticles || drawFluidsVelocityField || fluidHeatmapMode != FluidHeatmapMode::None;
	}
};

struct EngineState {
	std::vector<std::unique_ptr<Object>> Objects = {};
	std::vector<std::shared_ptr<Constraint>> Constraints = {};
};

struct ViewportRect { int x = 0, y = 0, width = 0, height = 0; };

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
		ConstraintEdit,
	};

	enum PhysicsMode {
		Pause,
		Stop,
		Simulate
	};

	bool isPlayer = false;
	bool isHeadless = false;

	InteractMode EngineInteractMode = EditorSelect;
	PhysicsMode EnginePhysicsMode = Stop;
	PhysicsMode EnginePrevPhysicsMode = Stop;
	bool pendingClose = false;

	Settings EngineSettings;

	EngineState SavedState;
	std::vector<std::unique_ptr<Object>> cachedSaveObjects;

	GLFWwindow* Window = nullptr;

	float fps;
	float headlessFps = 0.0f;
	float windowWidth;
	float windowHeight;
	float aspectRatio;
	float resolutionWidth = 1920;
	float resolutionHeight = 1080;
	float gameAspectRatio = (resolutionWidth / resolutionHeight);
	std::unordered_map<int, std::function<void()>> InteractModeChangedEvents;
	std::unordered_map<int, std::function<void()>> PhysicsModeChangedEvents;

	std::string editingScenePath = "";

	void Setup(GLFWwindow* window);
	void ProcessEngine(float delta);
	void SceneChangeEvent();
	void EngineChangeEvent();
	void SwitchInteractMode(InteractMode mode);
	void SwitchPhysicsMode(PhysicsMode mode);
	void SetGameResolution(float width, float height);
	ViewportRect GetPlayerViewportRect() const;
	void SerializeEngineSettings(BinaryWriter& w);
	void DeserializeEngineSettings(BinaryReader& r);
	int AddInteractModeChangedEvent(std::function<void()> func);
	int AddPhysicsModeChangedEvent(std::function<void()> func);
	void RemovePhysicsModeChangedEvent(int ID);
	void RemoveInteractModeChangedEvent(int ID);

	bool IsMainThread() const { return std::this_thread::get_id() == mainThreadId; }

	std::atomic<bool> liveTrainingRenderActive{ false };

	std::mutex headlessSimMutex;

	template <typename F>
	auto RunOnMainThread(F&& func) -> decltype(func()) {
		using R = decltype(func());

		if (IsMainThread()) {
			return func();
		}

		auto taskPromise = std::make_shared<std::promise<R>>();
		std::future<R> future = taskPromise->get_future();

		{
			std::lock_guard<std::mutex> lock(mainThreadQueueMutex);
			mainThreadTaskQueue.push([taskPromise, func = std::forward<F>(func)]() mutable {
				if constexpr (std::is_void_v<R>) {
					func();
					taskPromise->set_value();
				}
				else {
					taskPromise->set_value(func());
				}
				});
		}

		return future.get();
	}

	void ProcessPendingMainThreadTasks();

	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void WindowCloseCallback(GLFWwindow* window);
private:
	int CurrentInteractModeChangedID = -1;
	int CurrentPhysicsModeChangedID = -1;
	float time;
	float frameCount;

	std::mutex mainThreadQueueMutex;
	std::queue<std::function<void()>> mainThreadTaskQueue;

	std::thread::id mainThreadId;

	EngineManager() = default;

};