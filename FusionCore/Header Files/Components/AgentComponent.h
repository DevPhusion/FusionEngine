#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include <vector>
#include <cstdint>

class AgentComponent : public ComponentBase<AgentComponent>
{
public:
	AgentComponent(Object* parent);
	AgentComponent() = default;

	void AddObservation(float value);
	void AddObservationVec(const std::vector<float>& values);
	void SetObservation(const std::vector<float>& values);
	void ClearObservation();

	std::vector<float> GetAction() const { return pendingAction; }

	void AddReward(float delta);
	void SetReward(float value);
	void EndEpisode();

	void SetAction(std::vector<float> action) { pendingAction = std::move(action); }
	std::vector<float> GetObservation() { return observation; }
	float ConsumeReward();
	bool ConsumeDone();

	void SetActionSpace(int size, float low = -1.0f, float high = 1.0f);
	int GetActionSize() const { return actionSize; }
	float GetActionLow() const { return actionLow; }
	float GetActionHigh() const { return actionHigh; }

	uint32_t AgentId() const { return agentId; }

	virtual void Deactivate();
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);

private:
	std::vector<float> observation;
	std::vector<float> pendingAction;
	float rewardAccumulator = 0.0f;
	bool done = false;
	uint32_t agentId = 0;

	int actionSize = 2;
	float actionLow = -1.0f;
	float actionHigh = 1.0f;
};