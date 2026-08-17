#include "../../Header Files/Components/AgentComponent.h"

AgentComponent::AgentComponent(Object* parent) : ComponentBase<AgentComponent>(parent) {
	this->Name = "Agent Component";
}

void AgentComponent::Deactivate() {
	if (agentId != 0) {
		agentId = 0;
	}
	Component::Deactivate();
}

void AgentComponent::OnDelete() {
	Deactivate();
}

void AgentComponent::AddObservation(float value) { observation.push_back(value); }
void AgentComponent::AddObservationVec(const std::vector<float>& values) {
	observation.insert(observation.end(), values.begin(), values.end());
}
void AgentComponent::SetObservation(const std::vector<float>& values) { observation = values; }
void AgentComponent::ClearObservation() { observation.clear(); }

void AgentComponent::AddReward(float delta) { rewardAccumulator += delta; }
void AgentComponent::SetReward(float value) { rewardAccumulator = value; }
void AgentComponent::EndEpisode() { done = true; }

float AgentComponent::ConsumeReward() {
	float r = rewardAccumulator;
	rewardAccumulator = 0.0f;
	return r;
}

bool AgentComponent::ConsumeDone() {
	bool d = done;
	done = false;
	return d;
}

void AgentComponent::ProcessInspectorUI() {
	ImGui::Text("Agent ID: %u", agentId);
	ImGui::Text("Observation size: %d", (int)observation.size());
	ImGui::Text("Accumulated reward: %.3f", rewardAccumulator);
}

void AgentComponent::CopyTo(Object* other) {
	AgentComponent* target = other->GetComponent<AgentComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<AgentComponent>(other));
		target = other->GetComponent<AgentComponent>();
	}
	target->SetEnabled(Enabled);
}