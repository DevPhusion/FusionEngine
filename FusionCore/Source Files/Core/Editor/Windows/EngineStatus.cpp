#include "../../../../Header Files/Core/Editor/Windows/EngineStatus.h"
#include "../../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../../Header Files/Core/Files/FileDialog.h"
#include "../../../../Header Files/Core/Editor/HeadlessMonitor.h"
#include "../../../../Header Files/Core/Editor/EditorField.h"
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

	const bool rlPackageInstalled = PackageManager::getInstance().IsPackageInstalled("rl");

	const float rightGroupW = ButtonWidth("Settings") + style.ItemSpacing.x
		+ projectNameWidth + style.ItemSpacing.x
		+ ButtonWidth("Save") + style.ItemSpacing.x
		+ ButtonWidth("Export") + (rlPackageInstalled ? (style.ItemSpacing.x + ButtonWidth("Train")) : 0.0f);
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

	if (rlPackageInstalled) {
		ImGui::SameLine();
		{
			bool disableHeadless = EngineManager::getInstance().isHeadless
				|| FileManager::getInstance().currentProjectFile.empty()
				|| !ScriptManager::getInstance().IsReady();

			ImGui::BeginDisabled(disableHeadless);
			if (ImGui::Button("Train"))
				ImGui::OpenPopup("Train Settings");
			ImGui::EndDisabled();

			if (!ScriptManager::getInstance().IsReady()
				&& !FileManager::getInstance().currentProjectFile.empty()
				&& ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Python backend is still setting up (%s)",
					ScriptManager::getInstance().GetStatusMessage().c_str());
			}
		}
		ProcessTrainSettingsPopup();
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

		EditorField::CheckboxEngine("Draw background grid: ", "##Draw background grid", &settings.drawBackgroundGrid);

		EditorField::ColorEdit4Engine("Background color: ", "##Background color", &settings.backgroundColor.x);

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

		ImGui::Text("Broad phase mode: ");
		ImGui::SameLine();
		{
			const char* modeLabels[] = { "AABB", "Bounding Circle" };
			int current = static_cast<int>(settings.broadPhaseMode);
			ImGui::SetNextItemWidth(180.0f);
			if (EditorField::ComboEngine(nullptr, "##BroadPhaseMode", &current, modeLabels, IM_ARRAYSIZE(modeLabels))) {
				PhysicsEngine::getInstance().SetBroadPhaseMode(static_cast<BroadPhaseMode>(current));
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"AABB is more efficient for wide or tall objects.\n"
				"Bounding Circle can be cheaper/tighter for roughly round objects.\n"
				"Switching rebuilds the broad phase for every collidable object.");
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Debug");

		EditorField::CheckboxEngine("Draw object wire frame: ", "##Draw object wire frame", &settings.drawObjectWireframe);
		EditorField::CheckboxEngine("Draw broad phase bounding area: ", "##Draw broad phase bounding area", &settings.drawBroadPhaseBounds);
		EditorField::CheckboxEngine("Draw collision shapes: ", "##Draw collision shapes", &settings.drawCollisionShapes);
		EditorField::CheckboxEngine("Draw collision normals: ", "##Draw collision normals", &settings.drawCollisionNormals);
		EditorField::CheckboxEngine("Draw contact points: ", "##Draw contact points", &settings.drawContactPoints);
		EditorField::CheckboxEngine("Draw soft body point masses: ", "##Draw soft body point masses", &settings.drawSoftBodyPointMasses);
		EditorField::CheckboxEngine("Draw soft body springs: ", "##Draw soft body springs", &settings.drawSoftBodySprings);
		EditorField::CheckboxEngine("Draw virtual soft body proxies: ", "##Draw virtual soft body proxies", &settings.drawVirtualSoftBodyProxies);
		EditorField::CheckboxEngine("Draw fluids as particles: ", "##Draw fluids as particles", &settings.drawFluidsAsParticles);

		ImGui::BeginDisabled(!settings.drawFluidsAsParticles);
		ImGui::Indent();

		ImGui::Text("Fluid heatmap: ");
		ImGui::SameLine();
		{
			const char* heatmapLabels[] = { "None", "Velocity", "Density" };
			int current = static_cast<int>(settings.fluidHeatmapMode);
			ImGui::SetNextItemWidth(140.0f);
			if (EditorField::ComboEngine(nullptr, "##Fluid heatmap", &current, heatmapLabels, IM_ARRAYSIZE(heatmapLabels))) {
				settings.fluidHeatmapMode = static_cast<FluidHeatmapMode>(current);
			}
		}

		EditorField::CheckboxEngine("Draw velocity vector field: ", "##Draw velocity vector field", &settings.drawFluidsVelocityField);

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

		{
			char nameBuf[128];
			strncpy_s(nameBuf, sizeof(nameBuf), config.name.c_str(), _TRUNCATE);
			ImGui::Text("Name");
			ImGui::SetNextItemWidth(320.0f);
			if (EditorField::InputTextEngine(nullptr, "##ExportName", nameBuf, IM_ARRAYSIZE(nameBuf))) {
				config.name = nameBuf;
			}
		}

		{
			char versionBuf[32];
			strncpy_s(versionBuf, sizeof(versionBuf), config.version.c_str(), _TRUNCATE);
			ImGui::Text("Version");
			ImGui::SetNextItemWidth(160.0f);
			if (EditorField::InputTextEngine(nullptr, "##ExportVersion", versionBuf, IM_ARRAYSIZE(versionBuf))) {
				config.version = versionBuf;
			}
		}

		{
			char iconBuf[256];
			strncpy_s(iconBuf, sizeof(iconBuf), config.iconPath.c_str(), _TRUNCATE);
			ImGui::Text("Icon");
			ImGui::SetNextItemWidth(280.0f);
			if (EditorField::InputTextEngine(nullptr, "##ExportIcon", iconBuf, IM_ARRAYSIZE(iconBuf))) {
				config.iconPath = iconBuf;
			}
			ImGui::SameLine();
			if (ImGui::Button("Browse##Icon")) {
				auto opts = FileDialogOptions::ForExtension("Image", "png", "Choose Icon");
				if (auto path = FileDialog::ShowOpenDialog(opts)) {
					config.iconPath = *path;
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
		}

		{
			char authorBuf[128];
			strncpy_s(authorBuf, sizeof(authorBuf), config.author.c_str(), _TRUNCATE);
			ImGui::Text("Author");
			ImGui::SetNextItemWidth(280.0f);
			if (EditorField::InputTextEngine(nullptr, "##ExportAuthor", authorBuf, IM_ARRAYSIZE(authorBuf))) {
				config.author = authorBuf;
			}
		}

		ImGui::Dummy(ImVec2(0, 6));
		EditorField::CheckboxEngine("Auto zip export", "##AutoZipExport", &config.autoZipExport);

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

void EngineStatus::ProcessTrainSettingsPopup() {
	TrainConfig& cfg = HeadlessMonitor::getInstance().config;

	static bool saveDirInitialized = false;
	if (!saveDirInitialized && cfg.saveDir.empty() && !FileManager::getInstance().currentProjectFile.empty()) {
		std::filesystem::path projectDir =
			std::filesystem::path(FileManager::getInstance().currentProjectFile).parent_path();
		cfg.saveDir = (projectDir / "TrainedModels").string();
		saveDirInitialized = true;
	}

	const char* algos[] = { "PPO", "SAC", "A2C", "DDPG", "TD3" };
	int algoIndex = 0;
	for (int i = 0; i < IM_ARRAYSIZE(algos); i++) {
		if (cfg.algorithm == algos[i]) {
			algoIndex = i;
			break;
		}
	}

	const char* policies[] = {
		"MlpPolicy",
		"CnnPolicy",
		"MultiInputPolicy"
	};

	int policyIndex = 0;
	for (int i = 0; i < IM_ARRAYSIZE(policies); i++) {
		if (cfg.policy == policies[i]) {
			policyIndex = i;
			break;
		}
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(
		"Train Settings",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

		ImGui::SeparatorText("Train Settings");

		ImGui::Text("Algorithm");
		ImGui::SetNextItemWidth(200.0f);
		if (EditorField::ComboEngine(nullptr, "##Algo", &algoIndex, algos, IM_ARRAYSIZE(algos))) {
			cfg.algorithm = algos[algoIndex];
		}

		ImGui::Text("Policy");
		ImGui::SetNextItemWidth(200.0f);
		if (EditorField::ComboEngine(nullptr, "##Policy", &policyIndex, policies, IM_ARRAYSIZE(policies))) {
			cfg.policy = policies[policyIndex];
		}

		ImGui::Text("Total timesteps");
		ImGui::SetNextItemWidth(200.0f);
		{
			int steps = (int)cfg.totalTimesteps;
			if (EditorField::InputIntEngine(nullptr, "##Steps", &steps)) {
				cfg.totalTimesteps = std::max(1, steps);
			}
		}

		ImGui::Dummy(ImVec2(0, 8));

		ImGui::SeparatorText("File Settings");

		ImGui::Text("Model name");
		{
			char nameBuf[96];
			std::string display = cfg.modelName.empty() ? "trained_model" : cfg.modelName;

#if defined(_MSC_VER)
			strcpy_s(nameBuf, display.c_str());
#else
			strncpy(nameBuf, display.c_str(), sizeof(nameBuf) - 1);
			nameBuf[sizeof(nameBuf) - 1] = '\0';
#endif

			ImGui::SetNextItemWidth(200.0f);
			if (EditorField::InputTextEngine(nullptr, "##ModelName", nameBuf, IM_ARRAYSIZE(nameBuf))) {
				cfg.modelName = nameBuf;
			}

			ImGui::SameLine(0.0f, 4.0f);
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled(".zip");
		}

		ImGui::Text("Save folder");
		{
			char folderBuf[256];
			std::string display = cfg.saveDir.empty()
				? "(no folder selected)"
				: cfg.saveDir;

#if defined(_MSC_VER)
			strcpy_s(folderBuf, display.c_str());
#else
			strncpy(folderBuf, display.c_str(), sizeof(folderBuf) - 1);
			folderBuf[sizeof(folderBuf) - 1] = '\0';
#endif

			ImGui::SetNextItemWidth(280.0f);
			ImGui::InputText(
				"##SaveDir",
				folderBuf,
				IM_ARRAYSIZE(folderBuf),
				ImGuiInputTextFlags_ReadOnly
			);

			ImGui::SameLine();

			if (ImGui::Button("Browse##SaveDir")) {
				if (auto folder = FileDialog::ShowFolderDialog("Choose Save Folder")) {
					cfg.saveDir = *folder;
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
		}

		ImGui::Text("Start from model (optional)");
		{
			char modelBuf[256];
			std::string display = cfg.startFromModelPath.empty()
				? "(train from scratch)"
				: cfg.startFromModelPath;

#if defined(_MSC_VER)
			strcpy_s(modelBuf, display.c_str());
#else
			strncpy(modelBuf, display.c_str(), sizeof(modelBuf) - 1);
			modelBuf[sizeof(modelBuf) - 1] = '\0';
#endif

			ImGui::SetNextItemWidth(280.0f);
			ImGui::InputText(
				"##StartFromModel",
				modelBuf,
				IM_ARRAYSIZE(modelBuf),
				ImGuiInputTextFlags_ReadOnly
			);

			ImGui::SameLine();

			if (ImGui::Button("Browse##StartFromModel")) {
				auto opts = FileDialogOptions::ForExtension(
					"Trained Model",
					"zip",
					"Choose Trained Model"
				);

				if (auto path = FileDialog::ShowOpenDialog(opts)) {
					cfg.startFromModelPath =
						FileManager::getInstance().AbsoluteToVirtual(*path);

					EngineManager::getInstance().EngineChangeEvent();
				}
			}

			if (!cfg.startFromModelPath.empty()) {
				ImGui::SameLine();

				if (ImGui::Button("Clear##StartFromModel")) {
					cfg.startFromModelPath.clear();
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
		}

		ImGui::Dummy(ImVec2(0, 8));

		ImGui::SeparatorText("Periodic Shards");

		ImGui::Text("Shard interval (steps)");
		ImGui::SameLine(220.0f);
		ImGui::SetNextItemWidth(160.0f);
		{
			int interval = cfg.shardIntervalSteps;
			if (EditorField::InputIntEngine(nullptr, "##ShardInterval", &interval)) {
				cfg.shardIntervalSteps = std::max(0, interval);
			}
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"0 disables periodic shard saving. Otherwise a checkpoint "
				"(plus its normalization stats) is saved every N steps during this run."
			);
		}

		ImGui::BeginDisabled(cfg.shardIntervalSteps <= 0);

		ImGui::Text("Shards folder");
		{
			char shardBuf[256];
			std::string display = cfg.shardDir.empty()
				? "(defaults to <save folder>/Shards)"
				: cfg.shardDir;

#if defined(_MSC_VER)
			strcpy_s(shardBuf, display.c_str());
#else
			strncpy(shardBuf, display.c_str(), sizeof(shardBuf) - 1);
			shardBuf[sizeof(shardBuf) - 1] = '\0';
#endif

			ImGui::SetNextItemWidth(280.0f);
			ImGui::InputText(
				"##ShardDir",
				shardBuf,
				IM_ARRAYSIZE(shardBuf),
				ImGuiInputTextFlags_ReadOnly
			);

			ImGui::SameLine();

			if (ImGui::Button("Browse##ShardDir")) {
				if (auto folder = FileDialog::ShowFolderDialog("Choose Shards Folder")) {
					cfg.shardDir = *folder;
					EngineManager::getInstance().EngineChangeEvent();
				}
			}

			if (!cfg.shardDir.empty()) {
				ImGui::SameLine();

				if (ImGui::Button("Clear##ShardDir")) {
					cfg.shardDir.clear();
					EngineManager::getInstance().EngineChangeEvent();
				}
			}
		}

		ImGui::EndDisabled();

		ImGui::Dummy(ImVec2(0, 8));

		bool trainingFromModel = !cfg.startFromModelPath.empty();

		if (trainingFromModel) {
			ImGui::PushStyleColor(
				ImGuiCol_Text,
				ImVec4(0.90f, 0.70f, 0.25f, 1.0f)
			);

			ImGui::TextWrapped(
				"Advanced settings are not available when training from an existing model."
			);

			ImGui::PopStyleColor();
			ImGui::Dummy(ImVec2(0, 4));
		}

		ImGui::BeginDisabled(trainingFromModel);

		if (ImGui::Button("Advanced Settings", ImVec2(160, 0))) {
			ImGui::OpenPopup("Advanced Training Settings");
		}

		ImGui::EndDisabled();

		ProcessAdvancedTrainSettingsPopup();

		ImGui::Dummy(ImVec2(0, 8));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		bool canStart = !cfg.saveDir.empty();

		ImGui::BeginDisabled(!canStart);

		if (ImGui::Button("Start Training", ImVec2(140, 0))) {
			if (SaveCurrentWork()) {
				HeadlessMonitor::getInstance().StartTraining(cfg);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void EngineStatus::ProcessAdvancedTrainSettingsPopup() {
	TrainConfig& cfg = HeadlessMonitor::getInstance().config;
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Advanced Training Settings", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

		ImGui::TextDisabled("%s hyperparameters", cfg.algorithm.c_str());
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		if (cfg.algorithm == "PPO") {
			auto& s = cfg.ppoSettings;

			ImGui::PushID("Learning rate");
			ImGui::Text("Learning rate");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.learningRate, "%.5f");
			ImGui::PopID();

			ImGui::PushID("N steps");
			ImGui::Text("N steps");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.nSteps);
			ImGui::PopID();

			ImGui::PushID("Batch size");
			ImGui::Text("Batch size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.batchSize);
			ImGui::PopID();

			ImGui::PushID("N epochs");
			ImGui::Text("N epochs");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.nEpochs);
			ImGui::PopID();

			ImGui::PushID("Gamma");
			ImGui::Text("Gamma");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gamma, "%.4f");
			ImGui::PopID();

			ImGui::PushID("GAE lambda");
			ImGui::Text("GAE lambda");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gaeLambda, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Clip range");
			ImGui::Text("Clip range");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.clipRange, "%.3f");
			ImGui::PopID();

			ImGui::PushID("Entropy coef");
			ImGui::Text("Entropy coef");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.entCoef, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Value fn coef");
			ImGui::Text("Value fn coef");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.vfCoef, "%.3f");
			ImGui::PopID();

			ImGui::PushID("Max grad norm");
			ImGui::Text("Max grad norm");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.maxGradNorm, "%.3f");
			ImGui::PopID();
		}
		else if (cfg.algorithm == "A2C") {
			auto& s = cfg.a2cSettings;

			ImGui::PushID("Learning rate");
			ImGui::Text("Learning rate");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.learningRate, "%.5f");
			ImGui::PopID();

			ImGui::PushID("N steps");
			ImGui::Text("N steps");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.nSteps);
			ImGui::PopID();

			ImGui::PushID("Gamma");
			ImGui::Text("Gamma");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gamma, "%.4f");
			ImGui::PopID();

			ImGui::PushID("GAE lambda");
			ImGui::Text("GAE lambda");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gaeLambda, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Entropy coef");
			ImGui::Text("Entropy coef");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.entCoef, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Value fn coef");
			ImGui::Text("Value fn coef");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.vfCoef, "%.3f");
			ImGui::PopID();

			ImGui::PushID("Max grad norm");
			ImGui::Text("Max grad norm");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.maxGradNorm, "%.3f");
			ImGui::PopID();
		}
		else if (cfg.algorithm == "SAC") {
			auto& s = cfg.sacSettings;

			ImGui::PushID("Learning rate");
			ImGui::Text("Learning rate");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.learningRate, "%.5f");
			ImGui::PopID();

			ImGui::PushID("Buffer size");
			ImGui::Text("Buffer size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.bufferSize);
			ImGui::PopID();

			ImGui::PushID("Learning starts");
			ImGui::Text("Learning starts");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.learningStarts);
			ImGui::PopID();

			ImGui::PushID("Batch size");
			ImGui::Text("Batch size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.batchSize);
			ImGui::PopID();

			ImGui::PushID("Tau");
			ImGui::Text("Tau");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.tau, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Gamma");
			ImGui::Text("Gamma");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gamma, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Train freq");
			ImGui::Text("Train freq");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.trainFreq);
			ImGui::PopID();

			ImGui::PushID("Gradient steps");
			ImGui::Text("Gradient steps");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.gradientSteps);
			ImGui::PopID();
		}
		else if (cfg.algorithm == "DDPG") {
			auto& s = cfg.ddpgSettings;

			ImGui::PushID("Learning rate");
			ImGui::Text("Learning rate");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.learningRate, "%.5f");
			ImGui::PopID();

			ImGui::PushID("Buffer size");
			ImGui::Text("Buffer size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.bufferSize);
			ImGui::PopID();

			ImGui::PushID("Learning starts");
			ImGui::Text("Learning starts");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.learningStarts);
			ImGui::PopID();

			ImGui::PushID("Batch size");
			ImGui::Text("Batch size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.batchSize);
			ImGui::PopID();

			ImGui::PushID("Tau");
			ImGui::Text("Tau");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.tau, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Gamma");
			ImGui::Text("Gamma");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gamma, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Train freq");
			ImGui::Text("Train freq");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.trainFreq);
			ImGui::PopID();

			ImGui::PushID("Gradient steps");
			ImGui::Text("Gradient steps");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.gradientSteps);
			ImGui::PopID();
		}
		else if (cfg.algorithm == "TD3") {
			auto& s = cfg.td3Settings;

			ImGui::PushID("Learning rate");
			ImGui::Text("Learning rate");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.learningRate, "%.5f");
			ImGui::PopID();

			ImGui::PushID("Buffer size");
			ImGui::Text("Buffer size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.bufferSize);
			ImGui::PopID();

			ImGui::PushID("Learning starts");
			ImGui::Text("Learning starts");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.learningStarts);
			ImGui::PopID();

			ImGui::PushID("Batch size");
			ImGui::Text("Batch size");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.batchSize);
			ImGui::PopID();

			ImGui::PushID("Tau");
			ImGui::Text("Tau");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.tau, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Gamma");
			ImGui::Text("Gamma");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.gamma, "%.4f");
			ImGui::PopID();

			ImGui::PushID("Train freq");
			ImGui::Text("Train freq");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.trainFreq);
			ImGui::PopID();

			ImGui::PushID("Gradient steps");
			ImGui::Text("Gradient steps");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.gradientSteps);
			ImGui::PopID();

			ImGui::PushID("Policy delay");
			ImGui::Text("Policy delay");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputIntEngine(nullptr, "##val", &s.policyDelay);
			ImGui::PopID();

			ImGui::PushID("Target policy noise");
			ImGui::Text("Target policy noise");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.targetPolicyNoise, "%.3f");
			ImGui::PopID();

			ImGui::PushID("Target noise clip");
			ImGui::Text("Target noise clip");
			ImGui::SameLine(220.0f);
			ImGui::SetNextItemWidth(160.0f);
			EditorField::InputFloatEngine(nullptr, "##val", &s.targetNoiseClip, "%.3f");
			ImGui::PopID();
		}

		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		if (ImGui::Button("Close", ImVec2(120, 0))) {
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