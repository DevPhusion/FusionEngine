#include "../../../../Header Files/Core/Editor/Windows/EngineStatus.h"
#include "../../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../../Header Files/Core/Files/FileDialog.h"
#include "../../../../Header Files/Core/Editor/HeadlessMonitor.h"
#include <filesystem>

namespace {
	void DrawPlayIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
		float h = size * 0.3f;
		ImVec2 p1(center.x - h * 0.45f, center.y - h);
		ImVec2 p2(center.x - h * 0.45f, center.y + h);
		ImVec2 p3(center.x + h * 0.75f, center.y);
		drawList->AddTriangleFilled(p1, p2, p3, color);
	}

	void DrawPauseIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
		float barW = size * 0.18f;
		float barH = size * 0.5f;
		float gap = size * 0.12f;
		drawList->AddRectFilled(
			ImVec2(center.x - gap - barW, center.y - barH * 0.5f),
			ImVec2(center.x - gap, center.y + barH * 0.5f), color);
		drawList->AddRectFilled(
			ImVec2(center.x + gap, center.y - barH * 0.5f),
			ImVec2(center.x + gap + barW, center.y + barH * 0.5f), color);
	}

	void DrawStopIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
		float half = size * 0.26f;
		drawList->AddRectFilled(
			ImVec2(center.x - half, center.y - half),
			ImVec2(center.x + half, center.y + half), color, 1.5f);
	}

	enum class IconType { Play, Pause, Stop };

	bool IconButton(const char* id, IconType type, bool disabled, float dim = 20.0f,
		ImVec4 tint = ImVec4(0, 0, 0, -1.0f)) {
		ImGui::PushID(id);

		if (disabled)
			ImGui::BeginDisabled();

		bool pressed = ImGui::Button("##iconbtn", ImVec2(dim + 8.0f, dim + 8.0f));

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 rectMin = ImGui::GetItemRectMin();
		ImVec2 rectMax = ImGui::GetItemRectMax();
		ImVec2 center((rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f);

		ImU32 color;
		if (disabled)
			color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		else if (tint.w >= 0.0f)
			color = ImGui::GetColorU32(tint);
		else
			color = ImGui::GetColorU32(ImGuiCol_Text);

		switch (type) {
		case IconType::Play:  DrawPlayIcon(drawList, center, dim, color);  break;
		case IconType::Pause: DrawPauseIcon(drawList, center, dim, color); break;
		case IconType::Stop:  DrawStopIcon(drawList, center, dim, color);  break;
		}

		if (disabled)
			ImGui::EndDisabled();

		ImGui::PopID();
		return pressed && !disabled;
	}

	void Spacer(float width) {
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(width, 0.0f));
		ImGui::SameLine();
	}

	float ButtonWidth(const char* label) {
		return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	}

	bool SaveCurrentWork() {
		FileManager& FM = FileManager::getInstance();

		if (!FM.isProjectSaved && !FM.currentProjectFile.empty()) {
			FM.SaveProjectToFile(FM.currentProjectFile);
		}

		if (SceneManager::getInstance().IsActiveSceneDirty()) {
			return SceneTab::SaveActiveScene();
		}

		return true;
	}

	std::string GetProjectDisplayName() {
		const std::string& path = FileManager::getInstance().currentProjectFile;
		std::string baseName = path.empty()
			? "NewProject.fusion"
			: std::filesystem::path(path).filename().string();

		if (!FileManager::getInstance().isProjectSaved)
			baseName += "*";

		return baseName;
	}

	bool IsDirectoryEmpty(const std::string& folder) {
		std::error_code ec;
		return std::filesystem::is_empty(folder, ec) && !ec;
	}

	std::string TrimWhitespace(const std::string& s) {
		size_t start = s.find_first_not_of(" \t");
		size_t end = s.find_last_not_of(" \t");
		if (start == std::string::npos) return "";
		return s.substr(start, end - start + 1);
	}
}

EngineStatus::EngineStatus(std::string name) : EditorWindow(name) {
	EngineManager::getInstance().AddInteractModeChangedEvent([this]() {this->OnInteractModeChanged();});
	OnInteractModeChanged();
	InputManager::getInstance().SetKeyButtonCallback([this](int key, int scancode, int action, int mods) {OnKeyButtonPressed(key, scancode, action, mods);}, 999);
}

void EngineStatus::ProcessWindow() {
	ProcessUnsavedChangesPopup();

	ProjectExportManager::getInstance().Update();
	ProcessExportingPopup();

	if (hidden) return;

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1200, 46), ImGuiCond_FirstUseEver);

	ImGuiWindowClass statusWindowClass;
	statusWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
	ImGui::SetNextWindowClass(&statusWindowClass);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::Begin(name.c_str(), nullptr, flags);

	EngineManager::PhysicsMode mode = EngineManager::getInstance().EnginePhysicsMode;
	bool isStopped = mode == EngineManager::PhysicsMode::Stop;
	bool isSimulating = mode == EngineManager::PhysicsMode::Simulate;

	const ImGuiStyle& style = ImGui::GetStyle();
	const float fullWidth = ImGui::GetContentRegionAvail().x;
	const float rowStartX = ImGui::GetCursorPosX();

	const ImVec4 playGreen(0.30f, 0.80f, 0.35f, 1.0f);
	const ImVec4 stopRed(0.85f, 0.30f, 0.30f, 1.0f);

	const float iconDim = 20.0f;
	const float iconBtnW = iconDim + 8.0f;
	const float playbackGroupW = iconBtnW * 3.0f + style.ItemSpacing.x * 2.0f;
	const float playbackStartX = rowStartX + (fullWidth - playbackGroupW) * 0.5f;

	const float projectNameWidth = 160.0f;

	const float rightGroupW = ButtonWidth("Settings") + style.ItemSpacing.x
		+ projectNameWidth + style.ItemSpacing.x
		+ ButtonWidth("Save") + style.ItemSpacing.x
		+ ButtonWidth("Export") + style.ItemSpacing.x + ButtonWidth("Run Headless");
	const float rightStartX = rowStartX + fullWidth - rightGroupW;

	std::string fpsText = std::to_string(EngineManager::getInstance().fps) + " FPS";
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(fpsText.c_str());

	Spacer(16.0f);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Gizmo:");
	ImGui::SameLine();
	DrawGizmoModeSelector();

	ImGui::SameLine(playbackStartX);

	if (IconButton("play", IconType::Play, isSimulating, iconDim, playGreen)) {
		if (isStopped) {
			SceneManager& SM = SceneManager::getInstance();

			if (SM.IsActiveSceneDirty()) {
				if (!SceneTab::SaveActiveScene()) {
					ImGui::End();
					return; 
				}
			}

			EngineManager::getInstance().editingScenePath = SM.GetCurrentSceneFile();

			const std::string& mainScene = EngineManager::getInstance().EngineSettings.mainScenePath;
			if (!mainScene.empty() && SM.GetCurrentSceneFile() != mainScene) {
				std::error_code ec;
				if (std::filesystem::exists(mainScene, ec) && !ec) {
					SM.LoadSceneFromFile(mainScene);
				}
			}
		}
		EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
	}
	ImGui::SameLine();

	if (IconButton("pause", IconType::Pause, !isSimulating, iconDim)) {
		EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Pause);
	}
	ImGui::SameLine();

	if (IconButton("stop", IconType::Stop, isStopped, iconDim, stopRed)) {
		SceneManager& SM = SceneManager::getInstance();
		const std::string& editingScene = EngineManager::getInstance().editingScenePath;

		if (!editingScene.empty()) {
			SM.LoadSceneFromFile(editingScene);
		}
		else {
			SM.NewScene();
		}

		EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Stop);
	}

	ImGui::SameLine(rightStartX);

	if (ImGui::Button("Settings"))
		ImGui::OpenPopup("Settings");
	ProcessSettingsPopup();

	ImGui::SameLine();

	bool saveDisabled = !isStopped || FileManager::getInstance().isProjectSaved;
	ImGui::BeginDisabled(saveDisabled);
	if (ImGui::Button("Save")) {
		SaveCurrentWork();
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Export"))
		ImGui::OpenPopup("Export Project");
	ProcessExportPopup();

	ImGui::SameLine();
	{
		bool disableHeadless = HeadlessMonitor::getInstance().IsRunning()
			|| FileManager::getInstance().currentProjectFile.empty();
		ImGui::BeginDisabled(disableHeadless);
		if (ImGui::Button("Run Headless")) {
			if (SaveCurrentWork()) {
				std::string err;
				if (HeadlessMonitor::getInstance().Launch(FileManager::getInstance().currentProjectFile, err)) {
					headlessLaunchError.clear();
					EngineManager::getInstance().enteringHeadlessMonitor = true;
				}
				else {
					headlessLaunchError = err;
				}
			}
		}
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	{
		char projectNameBuf[128];
		std::string displayName = GetProjectDisplayName();
#if defined(_MSC_VER)
		strcpy_s(projectNameBuf, displayName.c_str());
#else
		strncpy(projectNameBuf, displayName.c_str(), sizeof(projectNameBuf) - 1);
		projectNameBuf[sizeof(projectNameBuf) - 1] = '\0';
#endif
		bool dirty = !FileManager::getInstance().isProjectSaved;
		const ImVec4 unsavedColor(0.95f, 0.65f, 0.25f, 1.0f);
		const ImVec4 savedColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

		ImGui::PushStyleColor(ImGuiCol_Text, dirty ? unsavedColor : savedColor);
		ImGui::SetNextItemWidth(projectNameWidth);
		ImGui::InputText("##ProjectName", projectNameBuf, IM_ARRAYSIZE(projectNameBuf), ImGuiInputTextFlags_ReadOnly);
		ImGui::PopStyleColor();
	}

	ImGui::End();
}

void EngineStatus::DrawGizmoModeSelector() {
	Gizmos* gizmos = Renderer::getInstance().gizmos;
	if (!gizmos) return;

	GizmosMode current = gizmos->currentGizmosMode;

	auto modeButton = [&](const char* label, GizmosMode mode) {
		bool isActive = (current == mode);
		if (isActive) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}
		if (ImGui::Button(label)) {
			gizmos->SwitchMode(mode);
		}
		if (isActive) {
			ImGui::PopStyleColor();
		}
		};

	modeButton("Move", GizmosMode::Move);
	ImGui::SameLine();
	modeButton("Rotate", GizmosMode::Rotate);
	ImGui::SameLine();
	modeButton("Scale", GizmosMode::Scale);
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
		if (ImGui::Checkbox("##Draw background grid", &settings.drawBackgroundGrid)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Background color: ");
		ImGui::SameLine();
		if (ImGui::ColorEdit4("##Background color", &settings.backgroundColor.x)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Main scene: ");
		ImGui::SameLine();
		{
			char sceneBuf[256];
			std::string current = settings.mainScenePath;
#if defined(_MSC_VER)
			strcpy_s(sceneBuf, current.c_str());
#else
			strncpy(sceneBuf, current.c_str(), sizeof(sceneBuf) - 1);
			sceneBuf[sizeof(sceneBuf) - 1] = '\0';
#endif
			ImGui::SetNextItemWidth(220.0f);
			ImGui::InputText("##Main scene", sceneBuf, IM_ARRAYSIZE(sceneBuf), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			if (ImGui::Button("Browse##MainScene")) {
				auto opts = FileDialogOptions::ForExtension("Fusion Scene", "fscene", "Choose Main Scene");
				if (auto path = FileDialog::ShowOpenDialog(opts)) {
					settings.mainScenePath = *path;
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
		}

		ImGui::Text("Game resolution: ");                                  
		ImGui::SameLine();                                                  
		{                                                                    
			int res[2] = {                                                  
				(int)EngineManager::getInstance().resolutionWidth,          
				(int)EngineManager::getInstance().resolutionHeight         
			};                                                               
			ImGui::SetNextItemWidth(160.0f);                                
			if (ImGui::InputInt2("##Game resolution", res)) {              
				EngineManager::getInstance().SetGameResolution(            
					(float)std::max(1, res[0]),                             
					(float)std::max(1, res[1]));    
				EngineManager::getInstance().EngineChangeEvent();
			}                                                                
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Debug");

		ImGui::Text("Draw object wire frame: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw object wire frame", &settings.drawObjectWireframe)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw broad phase bounding area: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw broad phase bounding area", &settings.drawBroadPhaseBounds)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw collision shapes: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw collision shapes", &settings.drawCollisionShapes)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw collision normals: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw collision normals", &settings.drawCollisionNormals)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw contact points: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw contact points", &settings.drawContactPoints)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw soft body point masses: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw soft body point masses", &settings.drawSoftBodyPointMasses)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw soft body springs: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw soft body springs", &settings.drawSoftBodySprings)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw virtual soft body proxies: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw virtual soft body proxies", &settings.drawVirtualSoftBodyProxies)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Draw fluids as particles: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw fluids as particles", &settings.drawFluidsAsParticles)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::BeginDisabled(!settings.drawFluidsAsParticles);
		ImGui::Indent();
		ImGui::Text("Fluid heatmap: ");
		ImGui::SameLine();
		{
			const char* heatmapLabels[] = { "None", "Velocity", "Density" };
			int current = static_cast<int>(settings.fluidHeatmapMode);
			ImGui::SetNextItemWidth(140.0f);
			if (ImGui::Combo("##Fluid heatmap", &current, heatmapLabels, IM_ARRAYSIZE(heatmapLabels))) {
				settings.fluidHeatmapMode = static_cast<FluidHeatmapMode>(current);
				EngineManager::getInstance().EngineChangeEvent();
			}
		}

		ImGui::Text("Draw velocity vector field: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Draw velocity vector field", &settings.drawFluidsVelocityField)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Unindent();
		ImGui::EndDisabled();

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

void EngineStatus::ProcessExportPopup() {
	ExportConfiguration& config = ProjectExportManager::getInstance().exportConfig;

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Export Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

		ImGui::Text("Export Folder");
		ImGui::TextWrapped("%s", config.exportFolder.empty() ? "(no folder selected)" : config.exportFolder.c_str());
		if (ImGui::Button("Browse...")) {
			if (auto folder = FileDialog::ShowFolderDialog("Choose Export Folder")) {
				config.exportFolder = *folder;
				EngineManager::getInstance().EngineChangeEvent();
			}
		}

		bool isPreviousExportFolder = false;
		{
			std::error_code ec;
			isPreviousExportFolder = std::filesystem::exists(std::filesystem::path(config.exportFolder) / "export_info.json", ec);
		}
		bool folderHasForeignContent = !config.exportFolder.empty() && !IsDirectoryEmpty(config.exportFolder) && !isPreviousExportFolder;
		if (folderHasForeignContent) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.65f, 0.25f, 1.0f));
			ImGui::TextWrapped("This folder isn't empty and wasn't previously exported to. Existing files may be overwritten.");
			ImGui::PopStyleColor();
		}

		ImGui::Dummy(ImVec2(0, 6));

		char nameBuf[128];
		strncpy_s(nameBuf, sizeof(nameBuf), config.name.c_str(), _TRUNCATE);
		ImGui::Text("Name");
		ImGui::SetNextItemWidth(320.0f);
		if (ImGui::InputText("##ExportName", nameBuf, IM_ARRAYSIZE(nameBuf))) {
			config.name = nameBuf;
			EngineManager::getInstance().EngineChangeEvent();
		}

		char versionBuf[32];
		strncpy_s(versionBuf, sizeof(versionBuf), config.version.c_str(), _TRUNCATE);
		ImGui::Text("Version");
		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::InputText("##ExportVersion", versionBuf, IM_ARRAYSIZE(versionBuf))) {
			config.version = versionBuf;
			EngineManager::getInstance().EngineChangeEvent();
		}

		char iconBuf[256];
		strncpy_s(iconBuf, sizeof(iconBuf), config.iconPath.c_str(), _TRUNCATE);
		ImGui::Text("Icon");
		ImGui::SetNextItemWidth(280.0f);
		if (ImGui::InputText("##ExportIcon", iconBuf, IM_ARRAYSIZE(iconBuf))) {
			config.iconPath = iconBuf;
			EngineManager::getInstance().EngineChangeEvent();
		}
		ImGui::SameLine();
		if (ImGui::Button("Browse##Icon")) {
			auto opts = FileDialogOptions::ForExtension("Image", "png", "Choose Icon");
			if (auto path = FileDialog::ShowOpenDialog(opts)) {
				config.iconPath = *path;
				EngineManager::getInstance().EngineChangeEvent();
			}
		}

		char authorBuf[128];
		strncpy_s(authorBuf, sizeof(authorBuf), config.author.c_str(), _TRUNCATE);
		ImGui::Text("Author");
		ImGui::SetNextItemWidth(280.0f);
		if (ImGui::InputText("##ExportAuthor", authorBuf, IM_ARRAYSIZE(authorBuf))) {
			config.author = authorBuf;
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Dummy(ImVec2(0, 6));
		if (ImGui::Checkbox("Auto zip export", &config.autoZipExport)) {
			EngineManager::getInstance().EngineChangeEvent();
		}

		if (!exportErrorMessage.empty()) {
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
			ImGui::TextWrapped("%s", exportErrorMessage.c_str());
			ImGui::PopStyleColor();
		}

		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		bool nameEmpty = TrimWhitespace(config.name).empty();
		bool canExport = !nameEmpty && !config.exportFolder.empty();

		ImGui::BeginDisabled(!canExport);
		if (ImGui::Button("Export", ImVec2(120, 0))) {
			if (ProjectExportManager::getInstance().StartExport(config)) {
				exportErrorMessage.clear();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			exportErrorMessage.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void EngineStatus::ProcessExportingPopup() {
	using Stage = ProjectExportManager::ExportStage;

	bool busy = ProjectExportManager::getInstance().IsBusy();
	Stage stage = ProjectExportManager::getInstance().GetStage();

	if (busy && !ImGui::IsPopupOpen("Exporting Project"))
		ImGui::OpenPopup("Exporting Project");

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Appearing);

	ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

	if (ImGui::BeginPopupModal("Exporting Project", nullptr, flags)) {
		if (!busy) {
			bool failed = (stage == Stage::Failed);
			ImGui::PushStyleColor(ImGuiCol_Text, failed
				? ImVec4(0.9f, 0.35f, 0.35f, 1.0f)
				: ImVec4(0.35f, 0.85f, 0.4f, 1.0f));
			ImGui::TextWrapped("%s", failed
				? ProjectExportManager::getInstance().GetLastError().c_str()
				: "Export complete.");
			ImGui::PopStyleColor();

			ImGui::Dummy(ImVec2(0, 8));
			if (ImGui::Button("Close", ImVec2(-1, 0))) {
				ImGui::CloseCurrentPopup();
			}
		}
		else {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
			float radius = 12.0f;
			ImVec2 spinnerCenter(pos.x + radius + 4.0f, pos.y + radius);
			float t = (float)ImGui::GetTime();

			for (int i = 0; i < 8; i++) {
				float angle = t * 6.0f + (float)i * (2.0f * 3.14159265f / 8.0f);
				float alpha = 0.2f + 0.8f * (float)i / 8.0f;
				ImVec2 p(spinnerCenter.x + cosf(angle) * radius, spinnerCenter.y + sinf(angle) * radius);
				drawList->AddCircleFilled(p, 2.5f, ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)));
			}

			ImGui::Dummy(ImVec2(radius * 2.0f + 8.0f, radius * 2.0f));
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Exporting project...");

			std::string status = ProjectExportManager::getInstance().GetStatusMessage();
			ImGui::Dummy(ImVec2(0, 4));
			ImGui::TextWrapped("%s", status.c_str());
		}

		ImGui::EndPopup();
	}
}

void EngineStatus::ProcessUnsavedChangesPopup() {
	bool anythingUnsaved = !FileManager::getInstance().isProjectSaved
		|| SceneManager::getInstance().AnySceneDirty();

	if (EngineManager::getInstance().pendingClose) {
		if (!anythingUnsaved) {
			EngineManager::getInstance().pendingClose = false;
			glfwSetWindowShouldClose(EngineManager::getInstance().Window, GLFW_TRUE);
		}
		else if (!ImGui::IsPopupOpen("Unsaved Changes")) {
			ImGui::OpenPopup("Unsaved Changes");
		}
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::Text("This project has unsaved changes.");
		ImGui::Text("What would you like to do?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		bool saveAndClose = ImGui::Button("Save and Close", ImVec2(140, 0));
		ImGui::PopStyleColor();
		ImGui::SetItemDefaultFocus();

		ImGui::SameLine();
		bool cancel = ImGui::Button("Cancel", ImVec2(120, 0));

		ImGui::SameLine();
		bool dontSave = ImGui::Button("Don't Save", ImVec2(120, 0));

		if (saveAndClose) {
			if (SaveCurrentWork()) {
				EngineManager::getInstance().pendingClose = false;
				glfwSetWindowShouldClose(EngineManager::getInstance().Window, GLFW_TRUE);
				ImGui::CloseCurrentPopup();
			}
		}

		if (dontSave) {
			EngineManager::getInstance().pendingClose = false;
			glfwSetWindowShouldClose(EngineManager::getInstance().Window, GLFW_TRUE);
			ImGui::CloseCurrentPopup();
		}

		if (cancel) {
			EngineManager::getInstance().pendingClose = false;
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

void EngineStatus::OnKeyButtonPressed(int key, int scancode, int action, int mods) {
	if (InputManager::getInstance().keys[GLFW_KEY_LEFT_CONTROL] && InputManager::getInstance().keys[GLFW_KEY_S]) {
		SaveCurrentWork();
	}
}