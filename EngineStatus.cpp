#include "EngineStatus.h"

EngineStatus::EngineStatus(std::string main) : EditorWindow(main) {
	EngineManager::getInstance().AddInteractModeChangedEvent([this]() {this->OnInteractModeChanged();});
	OnInteractModeChanged();
}

void EngineStatus::ProcessWindow() {
	if (hidden) return;

	ImGui::SetNextWindowPos(ImVec2(1510, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 130), ImGuiCond_FirstUseEver);

	ImGui::Begin(name.c_str());

	std::string fpsText = std::to_string(EngineManager::getInstance().fps) + " FPS";
	ImGui::Text(fpsText.c_str());
	ImGui::Text(InteractModeText.c_str());

	if (ImGui::Button("Settings"))
		ImGui::OpenPopup("Settings");

	ProcessSettingsPopup();

	ImGui::Text("Physics: ");
	ImGui::SameLine();
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Pause) {
		if (ImGui::Button("Run")) {
			EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
		}
	}
	else if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
		if (ImGui::Button("Pause")) {
			EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Pause);
		}
	}

	ImGui::End();
}

void EngineStatus::ProcessSettingsPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		Settings& settings = EngineManager::getInstance().EngineSettings;

		ImGui::SeparatorText("General");

		ImGui::Text("Draw background grid: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw background grid", &settings.drawBackgroundGrid);

		ImGui::Text("Background color: ");
		ImGui::SameLine();
		ImGui::ColorEdit4("##Background color", &settings.backgroundColor.x);

		ImGui::Spacing();
		ImGui::SeparatorText("Debug");

		ImGui::Text("Draw broad phase bounding area: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw broad phase bounding area", &settings.drawBroadPhaseBounds);

		ImGui::Text("Color collisions: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Color collisions", &settings.colorCollisions);

		ImGui::Text("Draw collision normals: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw collision normals", &settings.drawCollisionNormals);

		ImGui::Text("Draw contact points: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw contact points", &settings.drawContactPoints);

		ImGui::Text("Draw soft body point masses: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw soft body point masses", &settings.drawSoftBodyPointMasses);

		ImGui::Text("Draw soft body springs: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw soft body springs", &settings.drawSoftBodySprings);

		ImGui::Text("Draw virtual soft body proxies: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw virtual soft body proxies", &settings.drawVirtualSoftBodyProxies);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Close", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void EngineStatus::OnInteractModeChanged() {
	if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::AddVertex) {
		InteractModeText = "Interact Mode: ADD VERTEX";
	}
	if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::EditorSelect) {
		InteractModeText = "Interact Mode: MOUSE INTERACT";
	}
}