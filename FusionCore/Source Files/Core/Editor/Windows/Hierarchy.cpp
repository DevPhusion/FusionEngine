#include "../../../../Header Files/Core/Editor/Windows/Hierarchy.h"
#include "../../../../Header Files/Core/ObjectManager.h"

namespace {
	static ImU32 EyeIconColor(bool isHidden) {
		if (isHidden) return IM_COL32(128, 128, 128, 255); 
		return IM_COL32(255, 255, 255, 255); 
	}

	static void DrawEyeIcon(ImDrawList* drawList, ImVec2 center, float size, bool isHidden) {
		ImU32 color = EyeIconColor(isHidden);
		ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);

		float rx = size * 0.52f;
		float ry = size * 0.33f;

		const int segments = 24;
		ImVec2 points[segments];
		for (int i = 0; i < segments; i++) {
			float t = (2.0f * std::numbers::pi * i) / segments;
			points[i] = ImVec2(center.x + rx * cosf(t), center.y + ry * sinf(t));
		}
		drawList->AddConvexPolyFilled(points, segments, color);

		float ringOuterR = ry * 0.62f;
		float ringInnerR = ry * 0.30f;
		drawList->AddCircleFilled(center, ringOuterR, bgColor, 16);
		drawList->AddCircleFilled(center, ringInnerR, color, 16);

		if (isHidden) {
			float lineHalf = size * 0.58f;
			ImVec2 dir(0.82f, 0.57f);
			float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
			dir.x /= len; dir.y /= len;
			ImVec2 p0(center.x - dir.x * lineHalf, center.y + dir.y * lineHalf);
			ImVec2 p1(center.x + dir.x * lineHalf, center.y - dir.y * lineHalf);
			drawList->AddLine(p0, p1, color, size * 0.14f);
		}
	}

	static bool DrawEyeToggleButton(const char* strId, bool isHidden, float size) {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(strId, ImVec2(size, size));
		bool clicked = ImGui::IsItemClicked();

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
		DrawEyeIcon(drawList, center, size, isHidden);

		return clicked;
	}
}

Hierarchy::Hierarchy(std::string name) {
	this->name = name;
	InputManager::getInstance().SetKeyButtonCallback([this](int key, int scancode, int action, int mods) {this->OnKeyPressed(key, scancode, action, mods);}, 999);
}

void Hierarchy::DrawObjectNode(Object* currentObj, char* filter_buffer, char* renameBuffer) {
	if (currentObj == nullptr) return;
	if (currentObj->hideInHierarchy) return;
	if (filter_buffer[0] != '\0' && currentObj->name.find(filter_buffer) == std::string::npos) return;

	float availWidth = ImGui::GetContentRegionAvail().x;

	std::vector<std::unique_ptr<Object>>* allObjects = &(ObjectManager::getInstance().allObjects);

	std::vector<Object*> children;
	for (auto& o : *allObjects) {
		if (o && o.get() != currentObj && o->parent == currentObj && !o->hideInHierarchy) {
			children.push_back(o.get());
		}
	}

	if (currentObj->name.empty()) {
		currentObj->name = ObjectManager::getInstance().GenerateUniqueName("Object", currentObj);
	}

	ImGuiTreeNodeFlags item_flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_AllowOverlap |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (children.empty()) {
		item_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	if (EditorManager::getInstance().selectedObject == currentObj) {
		item_flags |= ImGuiTreeNodeFlags_Selected;
	}
	else if (currentObj->HasComponent<VertexComponent>()) {
		currentObj->GetComponent<VertexComponent>()->SetEnabled(false);
	}

	bool isRenamingThisNode = IsRenaming && EditorManager::getInstance().selectedObject == currentObj;

	std::string hiddenId = "##node_row_" + std::to_string(currentObj->id);
	ImGui::SetNextItemAllowOverlap();
	bool nodeOpen = ImGui::TreeNodeEx(hiddenId.c_str(), item_flags | ImGuiTreeNodeFlags_AllowOverlap);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		currentObj->GetComponent<MouseInteractComponent>()->SetSelectedPolygon(currentObj, true);
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		IsRenaming = true;
		currentObj->GetComponent<MouseInteractComponent>()->SetSelectedPolygon(currentObj, true);

#if defined(_MSC_VER)
		strcpy_s(renameBuffer, 256, currentObj->name.c_str());
#else
		strncpy(renameBuffer, currentObj->name.c_str(), 255);
		renameBuffer[255] = '\0';
#endif
	}

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		ImGui::SetDragDropPayload("HIERARCHY_OBJECT", &currentObj, sizeof(Object*));
		ImGui::Text("%s", currentObj->name.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT")) {
			Object* dragged = *(Object**)payload->Data;
			if (dragged != nullptr && dragged != currentObj) {
				bool isDescendant = false;
				Object* p = currentObj;
				while (p != nullptr) {
					if (p == dragged) {
						isDescendant = true;
						break;
					}
					p = p->parent;
				}

				if (!isDescendant) {
					dragged->SetParent(currentObj);
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();

	if (isRenamingThisNode) {
		if (IsRenaming) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

		ImGui::PushItemWidth(140.0f);
		if (ImGui::InputText("##renameField", renameBuffer, 256, input_flags)) {
			std::string desiredName = std::string(renameBuffer);
			if (desiredName.empty()) {
				desiredName = "Object";
			}
			currentObj->name = ObjectManager::getInstance().GenerateUniqueName(desiredName, currentObj);
			EngineManager::getInstance().EngineChangeEvent();
			IsRenaming = false;
		}
		ImGui::PopItemWidth();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())) {
			std::string desiredName = std::string(renameBuffer);
			if (desiredName.empty()) {
				desiredName = "Object";
			}
			currentObj->name = ObjectManager::getInstance().GenerateUniqueName(desiredName, currentObj);
			EngineManager::getInstance().EngineChangeEvent();
			IsRenaming = false;
		}
	}
	else {
		if (currentObj->hidden) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::Text("%s", currentObj->name.c_str());
			ImGui::PopStyleColor();
		}
		else {
			ImGui::Text("%s", currentObj->name.c_str());
		}
	}

	float eyeIconSize = ImGui::GetFrameHeight() * 0.6f;
	float rightPadding = ImGui::GetStyle().ScrollbarSize > 0.0f ? ImGui::GetStyle().ScrollbarSize + 2.0f : 4.0f;

	ImGui::SameLine(availWidth - eyeIconSize - rightPadding);

	std::string eyeId = "##eye_" + std::to_string(currentObj->id);
	if (DrawEyeToggleButton(eyeId.c_str(), currentObj->hidden, eyeIconSize)) {
		if (currentObj->hidden) currentObj->Show();
		else currentObj->Hide();
		EngineManager::getInstance().EngineChangeEvent();
	}

	if (!children.empty() && nodeOpen) {
		for (Object* child : children) {
			DrawObjectNode(child, filter_buffer, renameBuffer);
		}
		ImGui::TreePop();
	}
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

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT")) {
				Object* dragged = *(Object**)payload->Data;
				if (dragged != nullptr) {
					dragged->SetParent(nullptr);
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
			ImGui::EndDragDropTarget();
		}

		for (int i = 0; i < obj->size(); i++)
		{
			Object* currentObj = (*obj)[i].get();

			if (currentObj == nullptr) continue;
			if (currentObj->parent != nullptr) continue; // only root-level objects here; children are drawn recursively

			DrawObjectNode(currentObj, filter_buffer, renameBuffer);
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