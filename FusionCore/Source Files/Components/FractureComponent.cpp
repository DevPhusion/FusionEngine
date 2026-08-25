#include "../../Header Files/Components/FractureComponent.h"
#include "../../Header Files/Core/Editor/EditorField.h"

FractureComponent::FractureComponent(Object* parent) : ComponentBase<FractureComponent>(parent) {
	Name = "Fracture Component";
}

void FractureComponent::ProcessInspectorUI() {
	EditorField::CheckboxScene(parent, "Fracturable", "##Fracturable", &fracturable, [] {
		EngineManager::getInstance().SceneChangeEvent();
		});

	EditorField::InputFloatScene(parent, "Threshold", "##Threshold", &impulseThreshold, [] {
		EngineManager::getInstance().SceneChangeEvent();
		}, "%.3f Ns");

	EditorField::InputIntScene(parent, "Shard Count", "##ShardCount", &shardCount, [] {
		EngineManager::getInstance().SceneChangeEvent();
		});

	EditorField::InputFloatScene(parent, "Min Area", "##MinArea", &minFragmentArea, [] {
		EngineManager::getInstance().SceneChangeEvent();
		});

	EditorField::InputIntScene(parent, "Max Generation", "##MaxGen", &maxFractureGenerations, [] {
		EngineManager::getInstance().SceneChangeEvent();
		});

	if (!parent->HasComponent<RigidBodyComponent>()) {
		EditorField::InputFloatScene(parent, "Density", "##Density", &restDensity, [] {
			EngineManager::getInstance().SceneChangeEvent();
			});
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