#include "../../../../Header Files/Core/Editor/Windows/AddObjectWindow.h"
#include "../../../../Header Files/Core/ObjectManager.h"
#include "../../../../Header Files/Core/SceneManager.h"

namespace {
	struct TypeInfo { const char* name; const char* category; const char* description; };

	const TypeInfo kTypes[] = {
		{ "Object",        "General",      "An empty object with a transform. Use it as a container, or attach components to it yourself." },
		{ "Camera",        "General",      "Defines what the game view renders. Position and scale it to frame your scene." },
		{ "Rigid Box",     "Rigid Bodies", "A rectangular rigid body with collision and physics already set up." },
		{ "Rigid Circle",  "Rigid Bodies", "A circular rigid body with collision and physics already set up." },
		{ "Rigid Polygon", "Rigid Bodies", "A rigid body with a custom outline. You draw its vertices in the viewport after pressing Add." },
		{ "Soft Box",      "Soft Bodies",  "A rectangular deformable body built from point masses and springs." },
		{ "Soft Circle",   "Soft Bodies",  "A circular deformable body built from point masses and springs." },
		{ "Soft Polygon",  "Soft Bodies",  "A deformable body with a custom outline. You draw its vertices in the viewport after pressing Add." },
		{ "Fluid",         "Fluids & Gas", "A particle-based fluid." },
		{ "Gas",           "Fluids & Gas", "A particle-based gas." },
	};
	const char* kCategories[] = { "General", "Rigid Bodies", "Soft Bodies", "Fluids & Gas" };

	const TypeInfo* FindInfo(const std::string& name) {
		for (const TypeInfo& t : kTypes) if (name == t.name) return &t;
		return nullptr;
	}

	bool IsPolygonType(const std::string& name) {
		return name == "Rigid Polygon" || name == "Soft Polygon";
	}

	bool ContainsNoCase(const std::string& haystack, const char* needle) {
		std::string n(needle);
		auto it = std::search(haystack.begin(), haystack.end(), n.begin(), n.end(),
			[](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
		return it != haystack.end();
	}

	Object* LastCreatedObject() {
		auto& all = ObjectManager::getInstance().allObjects;
		return all.empty() ? nullptr : all.back().get();
	}
}

AddObjectWindow::AddObjectWindow(std::string name) {
	this->name = name;
	RefreshSceneList();
}

void AddObjectWindow::RefreshSceneList() {
	sceneFiles.clear();

	std::function<void(const std::string&)> scan = [&](const std::string& virtualPath) {
		for (auto& entry : FileManager::getInstance().GetDirectoryContents(virtualPath)) {
			if (entry.isDirectory) {
				std::string dirName = std::filesystem::path(entry.virtualPath).filename().string();
				if (FileManager::getInstance().IsDevOnlyDirectory(dirName)) continue;
				scan(entry.virtualPath);
			}
			else if (entry.iconType == ResourceIconType::Scene) {
				sceneFiles.push_back(entry.virtualPath);
			}
		}
		};
	scan(FileManager::getInstance().GetRootVirtualPath());
}

void AddObjectWindow::Show() {
	EditorWindow::Show();

	filter[0] = '\0';
	nameBuf[0] = '\0';

	RefreshSceneList();
	filter[0] = '\0';
	SelectedType.clear();
	SelectedScenePath.clear();
	justOpened = true;
}

void AddObjectWindow::ApplyName(Object* obj) {
	if (!obj) return;

	std::string name(nameBuf);
	const size_t start = name.find_first_not_of(" \t");
	if (start != std::string::npos) {
		name = name.substr(start, name.find_last_not_of(" \t") - start + 1);
	}
	else {
		if (!SelectedScenePath.empty())
			name = std::filesystem::path(SelectedScenePath).stem().string();
		else
			name = SelectedType;
	}

	if (name.empty()) return;

	obj->name = ObjectManager::getInstance().GenerateUniqueName(name, obj);
	EngineManager::getInstance().SceneChangeEvent();
}

void AddObjectWindow::Cancel() {
	if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::AddVertex) {
		Renderer::getInstance().polygonEditGizmos->EndEdit();
	}
	EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
	Hide();
}

void AddObjectWindow::CommitAdd() {
	ObjectManager& OM = ObjectManager::getInstance();

	if (!SelectedScenePath.empty()) {
		Object* root = SceneManager::getInstance().AddScene(SelectedScenePath, parent);
		ApplyName(root);
		Hide();
		return;
	}

	if (IsPolygonType(SelectedType)) {
		EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
		Renderer::getInstance().polygonEditGizmos->BeginEdit(nullptr);
		return;
	}

	if (SelectedType == "Object")            OM.AddObject(parent);
	else if (SelectedType == "Camera")       OM.AddCamera(parent);
	else if (SelectedType == "Rigid Box")    OM.AddBox(parent);
	else if (SelectedType == "Rigid Circle") OM.AddCircle(parent);
	else if (SelectedType == "Soft Box")     OM.AddSoftBox(parent);
	else if (SelectedType == "Soft Circle")  OM.AddSoftCircle(parent);
	else if (SelectedType == "Fluid")        OM.AddFluid(parent);
	else if (SelectedType == "Gas")          OM.AddGas(parent);
	else return;

	ApplyName(LastCreatedObject());
	Hide();
}

void AddObjectWindow::DrawVertexMode() {
	const auto& editedVerts = Renderer::getInstance().polygonEditGizmos->GetLocalVertices();
	const bool soft = SelectedType == "Soft Polygon";

	bool boldPushed = EditorTheme::PushBold();
	ImGui::Text("Draw %s", soft ? "Soft Polygon" : "Rigid Polygon");
	EditorTheme::PopBold(boldPushed);

	ImGui::TextDisabled("Click to add vertices, drag to move, right-click to remove.");
	ImGui::Spacing();
	ImGui::Text("Vertices: %d", (int)editedVerts.size());
	if (editedVerts.size() < 3) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "(need at least 3)");
	}

	ImGui::Spacing();
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("Name");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220.0f);
	ImGui::InputTextWithHint("##AddName", soft ? "Soft Polygon" : "Rigid Polygon", nameBuf, IM_ARRAYSIZE(nameBuf));
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginDisabled(editedVerts.size() < 3);
	if (ImGui::Button("Create", ImVec2(110, 0))) {
		if (soft) ObjectManager::getInstance().AddSoftPolygon(parent);
		else ObjectManager::getInstance().AddPolygon(parent);
		ApplyName(LastCreatedObject());
		EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
		Hide();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(110, 0))) {
		Cancel();
	}
}

void AddObjectWindow::DrawBrowser() {
	const ImGuiStyle& style = ImGui::GetStyle();
	const float footerH = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f + 6.0f;
	const float rowH = ImGui::GetFrameHeight() + 2.0f;

	bool commit = false;
	bool cancel = false;

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (justOpened) { ImGui::SetKeyboardFocusHere(); justOpened = false; }
	ImGui::InputTextWithHint("##AddFilter", "Search objects and scenes...", filter, IM_ARRAYSIZE(filter));

	ImGui::TextDisabled("Adding to:");
	ImGui::SameLine();
	ImGui::TextUnformatted(parent ? parent->name.c_str() : "Scene root");
	ImGui::Spacing();

	const float listW = 270.0f;
	const float childH = -footerH;

	ImGui::BeginChild("##AddList", ImVec2(listW, childH), ImGuiChildFlags_Borders);
	ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

	auto matches = [&](const std::string& s) { return filter[0] == '\0' || ContainsNoCase(s, filter); };
	bool anyShown = false;

	for (const char* category : kCategories) {
		bool headerDrawn = false;
		for (const std::string& type : ObjectTypes) {
			const TypeInfo* info = FindInfo(type);
			if (!info || std::string(info->category) != category || !matches(type)) continue;

			if (!headerDrawn) { ImGui::SeparatorText(category); headerDrawn = true; }
			anyShown = true;

			const bool selected = SelectedScenePath.empty() && SelectedType == type;
			ImGui::PushID(type.c_str());
			if (ImGui::Selectable(type.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, rowH))) {
				SelectedType = type;
				SelectedScenePath.clear();
				if (ImGui::IsMouseDoubleClicked(0)) commit = true;
			}
			ImGui::PopID();
		}
	}

	{
		const int activeIdx = SceneManager::getInstance().GetActiveIndex();
		const std::string activePath = activeIdx >= 0 ? SceneManager::getInstance().GetScene(activeIdx).filePath : "";

		bool headerDrawn = false;
		for (const std::string& scenePath : sceneFiles) {
			if (scenePath == activePath) continue;
			const std::string display = std::filesystem::path(scenePath).stem().string();
			if (!matches(display)) continue;

			if (!headerDrawn) { ImGui::SeparatorText("Scenes"); headerDrawn = true; }
			anyShown = true;

			ImGui::PushID(scenePath.c_str());
			if (ImGui::Selectable(display.c_str(), SelectedScenePath == scenePath,
				ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, rowH))) {
				SelectedScenePath = scenePath;
				SelectedType.clear();
				if (ImGui::IsMouseDoubleClicked(0)) commit = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", scenePath.c_str());
			ImGui::PopID();
		}

		if (!headerDrawn && filter[0] == '\0' && sceneFiles.empty()) {
			ImGui::SeparatorText("Scenes");
			ImGui::TextDisabled("No .fscene files in this project");
			anyShown = true;
		}
	}

	if (!anyShown) ImGui::TextDisabled("No results for \"%s\"", filter);

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##AddDetails", ImVec2(0, childH), ImGuiChildFlags_Borders);

	const TypeInfo* info = SelectedScenePath.empty() ? FindInfo(SelectedType) : nullptr;
	if (info) {
		bool boldPushed = EditorTheme::PushBold();
		ImGui::TextUnformatted(info->name);
		EditorTheme::PopBold(boldPushed);
		ImGui::TextDisabled("%s", info->category);
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextWrapped("%s", info->description);

		if (IsPolygonType(SelectedType)) {
			ImGui::Spacing();
			ImGui::TextColored(EditorTheme::Accent(), "Draws in the viewport");
		}
	}
	else if (!SelectedScenePath.empty()) {
		bool boldPushed = EditorTheme::PushBold();
		ImGui::TextUnformatted(std::filesystem::path(SelectedScenePath).stem().string().c_str());
		EditorTheme::PopBold(boldPushed);
		ImGui::TextDisabled("Scene");
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextWrapped("Adds an instance of this scene as a child object.");
		ImGui::Spacing();
		ImGui::TextDisabled("%s", SelectedScenePath.c_str());
	}
	else {
		ImGui::TextDisabled("Select an item to see details.");
		ImGui::TextDisabled("Double-click an item to add it right away.");
	}

	if (info || !SelectedScenePath.empty()) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextDisabled("Name");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##AddName", info ? info->name
			: std::filesystem::path(SelectedScenePath).stem().string().c_str(), nameBuf, IM_ARRAYSIZE(nameBuf));
	}

	ImGui::EndChild();

	ImGui::Separator();
	const bool canAdd = info != nullptr || !SelectedScenePath.empty();
	const float btnW = 110.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnW * 2.0f - style.ItemSpacing.x - style.WindowPadding.x);

	ImGui::BeginDisabled(!canAdd);
	ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::AccentDim());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorTheme::Accent());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorTheme::Accent());
	if (ImGui::Button("Add", ImVec2(btnW, 0))) commit = true;
	ImGui::PopStyleColor(3);
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(btnW, 0))) cancel = true;

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) cancel = true;
		if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) commit = true;
	}

	if (cancel) Cancel();
	else if (commit && canAdd) CommitAdd();
}

void AddObjectWindow::ProcessWindow() {
	if (hidden) return;

	const bool vertexMode =
		EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::AddVertex;
	ImGuiViewport* vp = ImGui::GetMainViewport();

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	if (vertexMode) {
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 60.0f),
			ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		flags |= ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;
	}
	else {
		ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_Always);
		flags |= ImGuiWindowFlags_NoResize;
	}

	bool open = true;
	if (ImGui::Begin("Add Object", &open, flags)) {
		if (vertexMode) DrawVertexMode();
		else DrawBrowser();
	}
	ImGui::End();

	if (!open && !hidden) Cancel();
}