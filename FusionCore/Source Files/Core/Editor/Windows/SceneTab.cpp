#include "../../../../Header Files/Core/Editor/Windows/SceneTab.h"
#include "../../../../Header Files/Core/Files/FileDialog.h"

SceneTab::SceneTab(std::string name) : EditorWindow(name) {}

bool SceneTab::SaveActiveScene() {
	SceneManager& SM = SceneManager::getInstance();
	if (SM.GetActiveIndex() < 0) return true;

	OpenScene& active = SM.GetScene(SM.GetActiveIndex());

	if (active.filePath.empty()) {
		auto opts = FileDialogOptions::ForExtension("Fusion Scene", "fscene", "Save Scene");
		opts.defaultFileName = "New Scene.fscene";
		if (auto path = FileDialog::ShowSaveDialog(opts)) {
			SM.SaveScene(*path);
			return true;
		}
		return false;
	}

	SM.SaveScene(active.filePath);
	return true;
}

void SceneTab::RequestClose(int index) {
	if (SceneManager::getInstance().GetScene(index).isDirty) {
		pendingCloseIndex = index;
		ImGui::OpenPopup("Unsaved Scene Changes");
	}
	else {
		SceneManager::getInstance().CloseSceneTab(index, true);
	}
}

void SceneTab::ProcessCloseConfirmPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Unsaved Scene Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::Text("This scene has unsaved changes.");
		ImGui::Text("What would you like to do?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		bool saveAndClose = ImGui::Button("Save and Close", ImVec2(150, 0));
		ImGui::PopStyleColor();
		ImGui::SetItemDefaultFocus();

		ImGui::SameLine();
		bool cancel = ImGui::Button("Cancel", ImVec2(120, 0));

		ImGui::SameLine();
		bool dontSave = ImGui::Button("Don't Save", ImVec2(120, 0));

		if (saveAndClose && pendingCloseIndex != -1) {
			SceneManager::getInstance().SwitchToScene(pendingCloseIndex); 

			if (SaveActiveScene()) {
				SceneManager::getInstance().CloseSceneTab(pendingCloseIndex, true);
				pendingCloseIndex = -1;
				ImGui::CloseCurrentPopup();
			}
		}

		if (dontSave && pendingCloseIndex != -1) {
			SceneManager::getInstance().CloseSceneTab(pendingCloseIndex, true);
			pendingCloseIndex = -1;
			ImGui::CloseCurrentPopup();
		}

		if (cancel) {
			pendingCloseIndex = -1;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void SceneTab::ProcessTabBar() {
	SceneManager& SM = SceneManager::getInstance();
	int focusIndex = SM.ConsumeFocusRequest();

	int closeRequestedIndex = -1;  

	if (ImGui::BeginTabBar("##SceneTabBar", ImGuiTabBarFlags_Reorderable)) {
		for (int i = 0; i < SM.GetSceneCount(); i++) {
			OpenScene& scene = SM.GetScene(i);

			bool open = true;
			ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
			if (i == focusIndex)
				flags |= ImGuiTabItemFlags_SetSelected;

			std::string label = scene.displayName;
			if (scene.isDirty) label += "*";
			label += "###scene" + std::to_string(scene.uid);

			const ImVec4 unsavedColor(0.95f, 0.65f, 0.25f, 1.0f);
			if (scene.isDirty) ImGui::PushStyleColor(ImGuiCol_Text, unsavedColor);

			if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
				if (ImGui::IsItemActivated() && i != SM.GetActiveIndex())
					SM.SwitchToScene(i);
				ImGui::EndTabItem();
			}

			if (scene.isDirty) ImGui::PopStyleColor();

			if (!open)
				closeRequestedIndex = i;  
		}

		if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
			SM.NewSceneTab();

		ImGui::EndTabBar();
	}

	if (closeRequestedIndex != -1)
		RequestClose(closeRequestedIndex);
}

void SceneTab::ProcessWindow() {
	if (EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Stop) return;

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	ImGuiWindowClass statusWindowClass;
	statusWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
	ImGui::SetNextWindowClass(&statusWindowClass);
	ImGui::Begin(name.c_str(), nullptr, flags);

	ProcessTabBar();
	ProcessCloseConfirmPopup();

	ImGui::End();
}