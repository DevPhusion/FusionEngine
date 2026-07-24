#include "../../../../Header Files/Core/Editor/Windows/Hierarchy.h"
#include "../../../../Header Files/Core/ObjectManager.h"

Hierarchy::Hierarchy(std::string name) {
	this->name = name;
	InputManager::getInstance().SetKeyButtonCallback([this](int key, int scancode, int action, int mods) {this->OnKeyPressed(key, scancode, action, mods);}, 999);
}

void Hierarchy::ProcessWindow() {
	if (hidden) return;

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350, 460), ImGuiCond_FirstUseEver);

	ImGui::Begin(name.c_str());

	ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;
	
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.18f));

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));

	if (ImGui::Button("+", ImVec2(24, 24))) {
		if (addObjectWindow == nullptr) {
			addObjectWindow = new AddObjectWindow("Add Object");
			EditorManager::getInstance().AddWindow(addObjectWindow);
		}
		else {
			addObjectWindow->Show();
		}
	}

	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	float search_bar_width = ImGui::GetContentRegionAvail().x - 28.0f;
	ImGui::SetNextItemWidth(search_bar_width);

	static char filter_buffer[256] = "";
	static char renameBuffer[256] = "";

	ImGui::InputTextWithHint("##FilterBar", "Filter..", filter_buffer, IM_ARRAYSIZE(filter_buffer));

	if (ImGui::TreeNodeEx("Root", root_flags)) {
		std::vector<std::unique_ptr<Object>>* obj = &(ObjectManager::getInstance().allObjects);
		for (int i = 0; i < obj->size(); i++)
		{
			Object* currentObj = (*obj)[i].get();

			if (currentObj == nullptr)
				return;

			if (currentObj->hidden || currentObj->name.find(filter_buffer) == std::string::npos) continue;

			ImGuiTreeNodeFlags item_flags = ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth;

			if (currentObj->name == "") {
				currentObj->name = "Unnamed Object " + std::to_string(i);
			}

			if (EditorManager::getInstance().selectedObject == currentObj) {
				item_flags |= ImGuiTreeNodeFlags_Selected;
			}
			else if (currentObj->HasComponent<VertexComponent>()) {
				currentObj->GetComponent<VertexComponent>()->SetEnabled(false);
			}

			bool isRenamingThisNode = IsRenaming && EditorManager::getInstance().selectedObject == currentObj;

			std::string hiddenId = "##node_row_" + std::to_string(i); // still render node but doesnt show text
			ImGui::TreeNodeEx(hiddenId.c_str(), item_flags);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				currentObj->GetComponent<MouseInteractComponent>()->SetSelectedPolygon(currentObj, true);
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				IsRenaming = true;
				currentObj->GetComponent<MouseInteractComponent>()->SetSelectedPolygon(currentObj, true);
				
				#if defined(_MSC_VER)
					strcpy_s(renameBuffer, currentObj->name.c_str());
				#else
					strncpy(renameBuffer, currentObj->Name.c_str(), sizeof(renameBuffer) - 1);
				#endif
			}

			ImGui::SameLine();

			if (isRenamingThisNode) {
				if (IsRenaming) {
					ImGui::SetKeyboardFocusHere();
				}

				ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

				ImGui::PushItemWidth(140.0f);
				if (ImGui::InputText("##renameField", renameBuffer, IM_ARRAYSIZE(renameBuffer), input_flags)) {
					currentObj->name = std::string(renameBuffer);
				}
				ImGui::PopItemWidth();

				if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())) {
					currentObj->name = std::string(renameBuffer);
					EngineManager::getInstance().EngineChangeEvent();
					IsRenaming = false;
				}
			}
			else {
				ImGui::Text("%s", currentObj->name.c_str());
			}
		}
		ImGui::TreePop();
	}

	ImGui::PopStyleVar();

	ImGui::End();
}

void Hierarchy::OnKeyPressed(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_DELETE && action == GLFW_PRESS) {
		if (EditorManager::getInstance().selectedObject != nullptr) {
			Object* obj = EditorManager::getInstance().selectedObject;
			EditorManager::getInstance().SetSelectedObject(nullptr);
			ObjectManager::getInstance().RemoveObject(obj);
		}
	}
	if (InputManager::getInstance().keys[GLFW_KEY_LEFT_CONTROL] && InputManager::getInstance().keys[GLFW_KEY_D]) {
		if (EditorManager::getInstance().selectedObject != nullptr) {
			Object* obj = EditorManager::getInstance().selectedObject;
			ObjectManager::getInstance().CopyObject(obj);
		}
	}
}