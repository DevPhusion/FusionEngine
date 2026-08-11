#include "../../Header Files/Components/FractureComponent.h"

FractureComponent::FractureComponent(Object* parent) : ComponentBase<FractureComponent>(parent) {
	Name = "Fracture Component";
}

void FractureComponent::ProcessInspectorUI() {
	ImGui::Text("Fracturable");
	ImGui::SameLine();
	if (ImGui::Checkbox("##Fracturable", &fracturable)) {
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Text("Threshold");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Threshold", &impulseThreshold, 0.0f, 0.0f, "%.3f Ns")) {
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Text("Shard Count");
	ImGui::SameLine();
	if (ImGui::InputInt("##ShardCount", &shardCount)) {
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Text("Min Area");
	ImGui::SameLine();
	if (ImGui::InputFloat("##MinArea", &minFragmentArea)) {
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Text("Max Generation");
	ImGui::SameLine();
	if (ImGui::InputInt("##MaxGen", &maxFractureGenerations)) {
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	if (!parent->HasComponent<RigidBodyComponent>()) {
		ImGui::Text("Density");
		ImGui::SameLine();
		if (ImGui::InputFloat("##Density", &restDensity)) {
			EngineManager::getInstance().SceneChangeEvent();
		}
		if (ImGui::IsItemActivated()) {
			EditorManager::getInstance().BeginEdit({ parent });
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			EditorManager::getInstance().EndEdit({ parent });
		}
	}
}

void FractureComponent::OnDelete() {

}

void FractureComponent::CopyTo(Object* other) {
	FractureComponent* target = other->GetComponent<FractureComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<FractureComponent>(other));
		target = other->GetComponent<FractureComponent>();
	}

	target->fracturable = fracturable;
	target->impulseThreshold = impulseThreshold;
	target->shardCount = shardCount;
	target->minFragmentArea = minFragmentArea;
	target->maxFractureGenerations = maxFractureGenerations;
	target->generation = generation;
	target->restDensity = restDensity;
	target->SetEnabled(Enabled);
}

std::unique_ptr<Component> FractureComponent::Clone(Object* parent) {
	std::unique_ptr<FractureComponent> comp = std::make_unique<FractureComponent>(parent);
	comp->fracturable = fracturable;
	comp->impulseThreshold = impulseThreshold;
	comp->shardCount = shardCount;
	comp->minFragmentArea = minFragmentArea;
	comp->maxFractureGenerations = maxFractureGenerations;
	comp->generation = generation;
	comp->restDensity = restDensity;
	comp->pendingEnabled = Enabled;
	comp->SetEnabled(false);
	return comp;
}

void FractureComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(fracturable);
	w.Write(impulseThreshold);
	w.Write(shardCount);
	w.Write(minFragmentArea);
	w.Write(maxFractureGenerations);
	w.Write(generation);
	w.Write(restDensity);
}

void FractureComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	fracturable = r.Read<bool>();
	impulseThreshold = r.Read<float>();
	shardCount = r.Read<int>();
	minFragmentArea = r.Read<float>();
	maxFractureGenerations = r.Read<int>();
	generation = r.Read<int>();
	restDensity = r.Read<float>();
}