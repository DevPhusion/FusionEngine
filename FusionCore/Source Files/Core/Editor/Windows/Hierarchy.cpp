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

	static ImU32 SceneIconColor(bool isOpen) {
		if (isOpen) return IM_COL32(120, 190, 255, 255); 
		return IM_COL32(200, 200, 200, 255);
	}

	static void DrawSceneIcon(ImDrawList* drawList, ImVec2 center, float size, bool isOpen) {
		ImU32 color = SceneIconColor(isOpen);

		float halfW = size * 0.34f;
		float halfH = size * 0.42f;
		float fold = size * 0.16f;
		float thickness = size * 0.09f;

		ImVec2 topLeft(center.x - halfW, center.y - halfH);
		ImVec2 topRight(center.x + halfW, center.y - halfH);
		ImVec2 bottomRight(center.x + halfW, center.y + halfH);
		ImVec2 bottomLeft(center.x - halfW, center.y + halfH);
		ImVec2 foldStart(topRight.x - fold, topRight.y);
		ImVec2 foldEnd(topRight.x, topRight.y + fold);

		ImVec2 outline[6] = { topLeft, foldStart, foldEnd, bottomRight, bottomLeft, topLeft };
		drawList->AddPolyline(outline, 6, color, ImDrawFlags_None, thickness);

		drawList->AddTriangleFilled(foldStart, topRight, foldEnd, color);

		float lineY1 = center.y - halfH * 0.05f;
		float lineY2 = center.y + halfH * 0.5f;
		drawList->AddLine(ImVec2(center.x - halfW * 0.55f, lineY1), ImVec2(center.x + halfW * 0.35f, lineY1), color, thickness * 0.7f);
		drawList->AddLine(ImVec2(center.x - halfW * 0.55f, lineY2), ImVec2(center.x + halfW * 0.35f, lineY2), color, thickness * 0.7f);
	}

	static bool DrawSceneToggleButton(const char* strId, float size, bool isOpen) {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(strId, ImVec2(size, size));
		bool clicked = ImGui::IsItemClicked();

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
		DrawSceneIcon(drawList, center, size, isOpen);

		return clicked;
	}

	bool ObjectMatchesFilterRecursive(Object* obj, const char* filter) {
		if (filter[0] == '\0') return true;
		if (obj->name.find(filter) != std::string::npos) return true;

		for (auto& o : ObjectManager::getInstance().allObjects) {
			if (o && o.get() != obj && o->parent == obj && !o->hideInHierarchy) {
				if (ObjectMatchesFilterRecursive(o.get(), filter)) return true;
			}
		}
		return false;
	}
}

Hierarchy::Hierarchy(std::string name) {
	this->name = name;
	InputManager::getInstance().SetKeyButtonCallback([this](int key, int scancode, int action, int mods) {this->OnKeyPressed(key, scancode, action, mods);}, 999);
}

void Hierarchy::DrawObjectNode(Object* currentObj, char* filter_buffer, char* renameBuffer) {
	if (currentObj == nullptr) return;
	if (currentObj->hideInHierarchy) return;
	if (!ObjectMatchesFilterRecursive(currentObj, filter_buffer)) return;

	bool isRoot = (currentObj->parent == nullptr);

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

	bool isRenamingThisNode = IsRenaming && EditorManager::getInstance().selectedObject == currentObj;

	bool filtering = filter_buffer[0] != '\0';
	bool selfMatches = !filtering || currentObj->name.find(filter_buffer) != std::string::npos;

	if (filtering && !selfMatches) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

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

	std::string ctxId = "##ctx_" + std::to_string(currentObj->id);
	bool nodeDeleted = false;
	if (ImGui::BeginPopupContextItem(ctxId.c_str())) {
		currentObj->GetComponent<MouseInteractComponent>()->SetSelectedPolygon(currentObj, true);

		if (ImGui::MenuItem("Add Child")) {
			if (addObjectWindow == nullptr) {
				addObjectWindow = new AddObjectWindow("Add Object");
				EditorManager::getInstance().AddWindow(addObjectWindow);
				addObjectWindow->parent = currentObj;
			}
			else {
				addObjectWindow->Show();
				addObjectWindow->parent = currentObj;
			}
		}
		if (ImGui::MenuItem("Rename")) {
			IsRenaming = true;
			currentObj->GetComponent<MouseInteractComponent>()->SetSelectedPolygon(currentObj, true);
#if defined(_MSC_VER)
			strcpy_s(renameBuffer, 256, currentObj->name.c_str());
#else
			strncpy(renameBuffer, currentObj->name.c_str(), 255);
			renameBuffer[255] = '\0';
#endif
		}
		if (!isRoot && ImGui::MenuItem("Delete")) {
			if (EditorManager::getInstance().selectedObject == currentObj) {
				EditorManager::getInstance().SetSelectedObject(nullptr);
			}
			ObjectManager::getInstance().RemoveObject(currentObj);
			EngineManager::getInstance().SceneChangeEvent();
			nodeDeleted = true;
		}
		ImGui::EndPopup();
	}

	if (nodeDeleted) {
		if (nodeOpen && !children.empty()) ImGui::TreePop();
		return;
	}

	if (!isRoot && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
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
					EngineManager::getInstance().SceneChangeEvent();
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
			EngineManager::getInstance().SceneChangeEvent();
			IsRenaming = false;
		}
		ImGui::PopItemWidth();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())) {
			std::string desiredName = std::string(renameBuffer);
			if (desiredName.empty()) {
				desiredName = "Object";
			}
			currentObj->name = ObjectManager::getInstance().GenerateUniqueName(desiredName, currentObj);
			EngineManager::getInstance().SceneChangeEvent();
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
	float iconSpacing = 4.0f;

	float eyeIconX = availWidth - eyeIconSize - rightPadding;

	if (currentObj->isSceneRoot) {
		float sceneIconSize = eyeIconSize;
		float sceneIconX = eyeIconX - sceneIconSize - iconSpacing;

		ImGui::SameLine(sceneIconX);

		bool isOpen = SceneManager::getInstance().FindSceneByPath(currentObj->sourceScenePath) != -1;
		std::string sceneId = "##scene_" + std::to_string(currentObj->id);

		if (DrawSceneToggleButton(sceneId.c_str(), sceneIconSize, isOpen) &&
			EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Stop) {
			SceneManager::getInstance().OpenSceneTab(currentObj->sourceScenePath);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(isOpen ? "Switch to Scene" : "Open Scene");
		}
	}

	ImGui::SameLine(eyeIconX);

	std::string eyeId = "##eye_" + std::to_string(currentObj->id);
	if (DrawEyeToggleButton(eyeId.c_str(), currentObj->hidden, eyeIconSize)) {
		if (currentObj->hidden) currentObj->Show();
		else currentObj->Hide();
		EngineManager::getInstance().SceneChangeEvent();
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
			addObjectWindow->parent = nullptr; 
		}
	}

	ImGui::PopStyleColor(3);
	ImGui::SameLine();

	float search_bar_width = ImGui::GetContentRegionAvail().x - 28.0f;
	ImGui::SetNextItemWidth(search_bar_width);

	static char filter_buffer[256] = "";
	static char renameBuffer[256] = "";
	ImGui::InputTextWithHint("##FilterBar", "Filter..", filter_buffer, IM_ARRAYSIZE(filter_buffer));

	Object* sceneRoot = ObjectManager::getInstance().GetSceneRoot();
	if (sceneRoot) {
		DrawObjectNode(sceneRoot, filter_buffer, renameBuffer);
	}
	else {
		ImGui::TextDisabled("Empty scene — use + to add a root object");
	}

	ImGui::PopStyleVar();
	ImGui::End();
}

void Hierarchy::OnKeyPressed(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_DELETE && action == GLFW_PRESS) {
		Object* obj = EditorManager::getInstance().selectedObject;
		if (obj != nullptr && obj->parent != nullptr) {
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