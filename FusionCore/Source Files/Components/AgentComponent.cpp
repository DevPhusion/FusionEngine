#include "../../Header Files/Components/AgentComponent.h"

namespace {
	const char* SpaceLabel(const AgentSpace& space) {
		return std::visit([](auto&& s) -> const char* {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, DiscreteSpace>) return "Discrete";
			else if constexpr (std::is_same_v<T, BoxSpace>) return "Box";
			else if constexpr (std::is_same_v<T, MultiDiscreteSpace>) return "MultiDiscrete";
			else return "MultiBinary";
			}, space);
	}
}

AgentComponent::AgentComponent(Object* parent) : ComponentBase<AgentComponent>(parent) {
	this->Name = "Agent Component";
}

void AgentComponent::Deactivate() {
	if (agentId != 0) agentId = 0;
	Component::Deactivate();
}

void AgentComponent::OnDelete() { Deactivate(); }

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

void AgentComponent::SetActionSpace(py::object space) {
	py::gil_scoped_acquire gil;
	actionSpace = std::move(space);
}
py::object AgentComponent::GetActionSpace() {
	py::gil_scoped_acquire gil;
	if (!actionSpace) actionSpace = BuildSpaceObject(actionSpaceConfig);
	return actionSpace;
}

void AgentComponent::SetObservationSpace(py::object space) {
	py::gil_scoped_acquire gil;
	observationSpace = std::move(space);
}
py::object AgentComponent::GetObservationSpace() {
	py::gil_scoped_acquire gil;
	if (!observationSpace) {
		observationSpace = useCustomObservationSpace
			? BuildSpaceObject(observationSpaceConfig)
			: py::none();  
	}
	return observationSpace;
}

void AgentComponent::SetAction(py::object action) {
	py::gil_scoped_acquire gil;
	pendingAction = std::move(action);
}
py::object AgentComponent::GetAction() {
	py::gil_scoped_acquire gil;
	if (!pendingAction) pendingAction = py::none();
	return pendingAction;
}

py::object AgentComponent::BuildSpaceObject(const AgentSpace& space) {
	py::gil_scoped_acquire gil;
	try {
		using namespace pybind11::literals;
		py::object spacesModule = py::module_::import("gymnasium").attr("spaces");

		return std::visit([&](auto&& s) -> py::object {
			using T = std::decay_t<decltype(s)>;

			if constexpr (std::is_same_v<T, DiscreteSpace>) {
				return spacesModule.attr("Discrete")(std::max(1, s.n));
			}
			else if constexpr (std::is_same_v<T, BoxSpace>) {
				py::object npFloat32 = py::module_::import("numpy").attr("float32");
				return spacesModule.attr("Box")(
					"low"_a = s.low,
					"high"_a = s.high,
					"shape"_a = py::make_tuple(std::max(1, s.size)),
					"dtype"_a = npFloat32);
			}
			else if constexpr (std::is_same_v<T, MultiDiscreteSpace>) {
				std::vector<int> nvec = s.nvec.empty() ? std::vector<int>{2} : s.nvec;
				return spacesModule.attr("MultiDiscrete")(nvec);
			}
			else { 
				return spacesModule.attr("MultiBinary")(std::max(1, s.n));
			}
			}, space);
	}
	catch (const py::error_already_set& e) {
		Console::PrintError(std::string("AgentComponent: failed to build space: ") + e.what());
		return py::none();
	}
}

void AgentComponent::ProcessSpaceConfigUI(const char* idPrefix, AgentSpace& space,
	const std::function<void()>& onChanged) {
	ImGui::PushID(idPrefix);

	ImGui::Text("  Type");
	ImGui::SameLine();

	if (ImGui::BeginCombo("##SpaceType", SpaceLabel(space))) {
		if (ImGui::Selectable("Discrete", std::holds_alternative<DiscreteSpace>(space))) {
			EditorManager::getInstance().BeginEdit({ parent });
			space = DiscreteSpace{};
			onChanged();
			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("Box", std::holds_alternative<BoxSpace>(space))) {
			EditorManager::getInstance().BeginEdit({ parent });
			space = BoxSpace{};
			onChanged();
			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("MultiDiscrete", std::holds_alternative<MultiDiscreteSpace>(space))) {
			EditorManager::getInstance().BeginEdit({ parent });
			space = MultiDiscreteSpace{};
			onChanged();
			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("MultiBinary", std::holds_alternative<MultiBinarySpace>(space))) {
			EditorManager::getInstance().BeginEdit({ parent });
			space = MultiBinarySpace{};
			onChanged();
			EditorManager::getInstance().EndEdit({ parent });
		}
		ImGui::EndCombo();
	}

	std::visit([&](auto&& s) {
		using T = std::decay_t<decltype(s)>;

		if constexpr (std::is_same_v<T, DiscreteSpace>) {
			ImGui::Text("  n");
			ImGui::SameLine();
			int n = s.n;
			if (ImGui::InputInt("##DiscreteN", &n)) {
				s.n = std::max(1, n);
				onChanged();
			}
			if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
			if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
		}
		else if constexpr (std::is_same_v<T, BoxSpace>) {
			float bounds[2] = { s.low, s.high };
			ImGui::Text("  Low / High");
			ImGui::SameLine();
			if (ImGui::InputFloat2("##BoxBounds", bounds)) {
				s.low = bounds[0];
				s.high = std::max(bounds[0], bounds[1]);
				onChanged();
			}
			if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
			if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });

			ImGui::Text("  Size");
			ImGui::SameLine();
			int size = s.size;
			if (ImGui::InputInt("##BoxSize", &size)) {
				s.size = std::max(1, size);
				onChanged();
			}
			if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
			if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
		}
		else if constexpr (std::is_same_v<T, MultiDiscreteSpace>) {
			ImGui::Text("  Values (n per dimension)");
			int removeIndex = -1;
			for (int i = 0; i < (int)s.nvec.size(); i++) {
				ImGui::PushID(i);
				int v = s.nvec[i];
				ImGui::SetNextItemWidth(200.0f);
				if (ImGui::InputInt("##NVecEntry", &v)) {
					s.nvec[i] = std::max(1, v);
					onChanged();
				}
				if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
				if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });

				ImGui::SameLine();
				if (ImGui::SmallButton("x")) {
					removeIndex = i;
				}
				ImGui::PopID();
			}
			if (removeIndex != -1 && s.nvec.size() > 1) {
				EditorManager::getInstance().BeginEdit({ parent });
				s.nvec.erase(s.nvec.begin() + removeIndex);
				onChanged();
				EditorManager::getInstance().EndEdit({ parent });
			}
			if (ImGui::SmallButton("+ Add dimension")) {
				EditorManager::getInstance().BeginEdit({ parent });
				s.nvec.push_back(2);
				onChanged();
				EditorManager::getInstance().EndEdit({ parent });
			}
		}
		else { 
			ImGui::Text("  n");
			ImGui::SameLine();
			int n = s.n;
			if (ImGui::InputInt("##MultiBinaryN", &n)) {
				s.n = std::max(1, n);
				onChanged();
			}
			if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
			if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
		}
		}, space);

	ImGui::PopID();
}

void AgentComponent::ProcessInspectorUI() {
	if (!Py_IsInitialized()) return; 

	py::gil_scoped_acquire gil;

	ImGui::Text("Agent ID");
	ImGui::SameLine();
	int agentIdValue = (int)agentId;
	ImGui::InputInt("##AgentId", &agentIdValue, 0, 0, ImGuiInputTextFlags_ReadOnly);

	ImGui::Text("Accumulated reward");
	ImGui::SameLine();
	float rewardValue = rewardAccumulator;
	ImGui::InputFloat("##AccumulatedReward", &rewardValue, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::Separator();

	ImGui::Text("Action Space");
	ProcessSpaceConfigUI("ActionSpaceConfig", actionSpaceConfig, [this]() {
		SetActionSpace(BuildSpaceObject(actionSpaceConfig));
		EngineManager::getInstance().SceneChangeEvent();
		});

	ImGui::Separator();

	ImGui::Text("Observation");
	ImGui::SameLine();
	bool useCustom = useCustomObservationSpace;
	if (ImGui::Checkbox("##ObsSpaceCustom", &useCustom)) {
		EditorManager::getInstance().BeginEdit({ parent });
		useCustomObservationSpace = useCustom;
		SetObservationSpace(useCustomObservationSpace
			? BuildSpaceObject(observationSpaceConfig)
			: py::none());
		EngineManager::getInstance().SceneChangeEvent();
		EditorManager::getInstance().EndEdit({ parent });
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Inferred from observation length if unchecked, otherwise use a custom space below");
	}

	ImGui::Text("  Observation size");
	ImGui::SameLine();
	int observationSizeValue = (int)observation.size();
	ImGui::InputInt("##ObservationSize", &observationSizeValue, 0, 0, ImGuiInputTextFlags_ReadOnly);

	if (useCustomObservationSpace) {
		ProcessSpaceConfigUI("ObservationSpaceConfig", observationSpaceConfig, [this]() {
			SetObservationSpace(BuildSpaceObject(observationSpaceConfig));
			EngineManager::getInstance().SceneChangeEvent();
			});
	}
}

void AgentComponent::CopyTo(Object* other) {
	AgentComponent* target = other->GetComponent<AgentComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<AgentComponent>(other));
		target = other->GetComponent<AgentComponent>();
	}
	target->SetEnabled(Enabled);
	target->actionSpaceConfig = actionSpaceConfig;
	target->observationSpaceConfig = observationSpaceConfig;
	target->useCustomObservationSpace = useCustomObservationSpace;
	target->SetActionSpace(GetActionSpace());
	target->SetObservationSpace(GetObservationSpace());
}

void AgentComponent::SerializeSpace(BinaryWriter& w, const AgentSpace& space) {
	w.Write(static_cast<uint8_t>(space.index()));

	std::visit([&](auto&& s) {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, DiscreteSpace>) {
			w.Write(s.n);
		}
		else if constexpr (std::is_same_v<T, BoxSpace>) {
			w.Write(s.low);
			w.Write(s.high);
			w.Write(s.size);
		}
		else if constexpr (std::is_same_v<T, MultiDiscreteSpace>) {
			w.WriteArray(s.nvec);
		}
		else { 
			w.Write(s.n);
		}
		}, space);
}

void AgentComponent::DeserializeSpace(BinaryReader& r, AgentSpace& space) {
	uint8_t index = r.Read<uint8_t>();

	switch (index) {
	case 0: { 
		DiscreteSpace s;
		s.n = r.Read<int>();
		space = s;
		break;
	}
	case 1: { 
		BoxSpace s;
		s.low = r.Read<float>();
		s.high = r.Read<float>();
		s.size = r.Read<int>();
		space = s;
		break;
	}
	case 2: { 
		MultiDiscreteSpace s;
		s.nvec = r.ReadArray<int>();
		space = s;
		break;
	}
	default: { 
		MultiBinarySpace s;
		s.n = r.Read<int>();
		space = s;
		break;
	}
	}
}

void AgentComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	SerializeSpace(w, actionSpaceConfig);
	SerializeSpace(w, observationSpaceConfig);
	w.Write(static_cast<uint8_t>(useCustomObservationSpace ? 1 : 0));
}

void AgentComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	
	DeserializeSpace(r, actionSpaceConfig);
	DeserializeSpace(r, observationSpaceConfig);
	useCustomObservationSpace = r.Read<uint8_t>() != 0;

	if (Py_IsInitialized()) {
		py::gil_scoped_acquire gil;
		actionSpace = py::object();
		observationSpace = py::object();
	}
}