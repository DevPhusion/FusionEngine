#include "../../../../Header Files/Core/Editor/Windows/AddObjectWindow.h"
#include "../../../../Header Files/Core/ObjectManager.h"

AddObjectWindow::AddObjectWindow(std::string name) {
	this->name = name;
}

void AddObjectWindow::ProcessWindow() {
	if (hidden) return;

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

	ImGui::Begin(name.c_str(), nullptr, window_flags);

	ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::EditorSelect) {
		if (ImGui::TreeNodeEx("Add Object", root_flags)) {
		
			for (int i = 0; i < ObjectTypes.size(); i++)
			{
				ImGuiTreeNodeFlags item_flags = ImGuiTreeNodeFlags_Leaf |
					ImGuiTreeNodeFlags_NoTreePushOnOpen |
					ImGuiTreeNodeFlags_SpanAvailWidth;

				if (SelectedType == ObjectTypes[i]) {
					item_flags |= ImGuiTreeNodeFlags_Selected;
				}

				ImGui::TreeNodeEx(ObjectTypes[i].c_str(), item_flags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
					SelectedType = ObjectTypes[i];
				}
			}

			ImGui::TreePop();
		}
	}
	else {
		const auto& editedVerts = Renderer::getInstance().polygonEditGizmos->GetLocalVertices();
		ImGui::Text("Click to add vertices, drag to move, right-click to remove.");
		ImGui::Text("Vertices: %d", (int)editedVerts.size());
	}

	float button_width = 100.0f;
	int button_count = 2;
	float item_spacing = ImGui::GetStyle().ItemSpacing.x;

	float total_row_width = (button_width * button_count) + (item_spacing * (button_count - 1));

	float available_width = ImGui::GetContentRegionAvail().x;
	float start_pos_x = (available_width - total_row_width) * 0.5f;

	if (start_pos_x > 0.0f) {
		ImGui::SetCursorPosX(start_pos_x);
	}

	if (ImGui::Button("Add", ImVec2(button_width, 0.0f))) {
		if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::EditorSelect) {
			if (SelectedType == "Object") {
				ObjectManager::getInstance().AddObject(parent);
				Hide();
			}
			else if (SelectedType == "Camera") {
				ObjectManager::getInstance().AddCamera(parent);
				Hide();
			}
			else if (SelectedType == "Rigid Box") {
				ObjectManager::getInstance().AddBox(parent);
				Hide();
			}
			else if (SelectedType == "Rigid Circle") {
				ObjectManager::getInstance().AddCircle(parent);
				Hide();
			}
			else if (SelectedType == "Rigid Polygon") {
				EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
				Renderer::getInstance().polygonEditGizmos->BeginEdit(nullptr);
			}
			else if (SelectedType == "Soft Box") {
				ObjectManager::getInstance().AddSoftBox(parent);
				Hide();
			}
			else if (SelectedType == "Soft Circle") {
				ObjectManager::getInstance().AddSoftCircle(parent);
				Hide();
			}
			else if (SelectedType == "Soft Polygon") {
				EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
				Renderer::getInstance().polygonEditGizmos->BeginEdit(nullptr);
			}
			else if (SelectedType == "Fluid") {
				ObjectManager::getInstance().AddFluid(parent);
				Hide();
			}
		}
		else {
			if (SelectedType == "Rigid Polygon") {
				ObjectManager::getInstance().AddPolygon(parent);
			}
			else if (SelectedType == "Soft Polygon") {
				ObjectManager::getInstance().AddSoftPolygon(parent);
			}
			EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
			Hide();
		}
	}

	ImGui::SameLine(); 

	if (ImGui::Button("Close", ImVec2(button_width, 0.0f))) {
		if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::AddVertex) {
			Renderer::getInstance().polygonEditGizmos->EndEdit();
		}
		EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
		Hide();
	}

	ImGui::End();
}