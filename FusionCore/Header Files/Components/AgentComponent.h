#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include "../Core/EngineManager.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>
#include <variant>
#include <cstdint>

namespace py = pybind11;

struct DiscreteSpace {
	int n = 2;
};

struct BoxSpace {
	float low = -1.0f;
	float high = 1.0f;
	int size = 1;  
};

struct MultiDiscreteSpace {
	std::vector<int> nvec = { 2 };
};

struct MultiBinarySpace {
	int n = 2;
};

using AgentSpace = std::variant<DiscreteSpace, BoxSpace, MultiDiscreteSpace, MultiBinarySpace>;

class AgentComponent : public ComponentBase<AgentComponent>
{
public:
	AgentComponent(Object* parent);
	AgentComponent() = default;

	void AddObservation(float value);
	void AddObservationVec(const std::vector<float>& values);
	void SetObservation(const std::vector<float>& values);
	void ClearObservation();
	std::vector<float> GetObservation() { return observation; }

	void AddReward(float delta);
	void SetReward(float value);
	void EndEpisode();

	void SetActionSpace(py::object space);
	py::object GetActionSpace();

	void SetObservationSpace(py::object space);
	py::object GetObservationSpace();

	void SetAction(py::object action);
	py::object GetAction();

	float ConsumeReward();
	bool ConsumeDone();

	uint32_t AgentId() const { return agentId; }

	virtual void Deactivate();
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

private:
	std::vector<float> observation;
	float rewardAccumulator = 0.0f;
	bool done = false;
	uint32_t agentId = 0;

	py::object actionSpace;
	py::object observationSpace;
	py::object pendingAction;

	AgentSpace actionSpaceConfig = DiscreteSpace{};
	AgentSpace observationSpaceConfig = DiscreteSpace{};

	bool useCustomObservationSpace = false;

	py::object BuildSpaceObject(const AgentSpace& space);
	void ProcessSpaceConfigUI(const char* idPrefix, AgentSpace& space, const std::function<void()>& onChanged);

	void SerializeSpace(BinaryWriter& w, const AgentSpace& space);
	void DeserializeSpace(BinaryReader& r, AgentSpace& space);
};