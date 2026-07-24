#include "../../../../Header Files/Core/Editor/Windows/EngineStatus.h"
#include "../../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../../Header Files/Core/Files/FileDialog.h"
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

	std::string GetProjectDisplayName() {
		const std::string& path = FileManager::getInstance().currentProjectFile;
		std::string baseName = path.empty()
			? "NewProject.fusion"
			: std::filesystem::path(path).filename().string();

		if (!FileManager::getInstance().isSaved)
			baseName += "*";

		return baseName;
	}
}

EngineStatus::EngineStatus(std::string name) : EditorWindow(name) {
	EngineManager::getInstance().AddInteractModeChangedEvent([this]() {this->OnInteractModeChanged();});
	OnInteractModeChanged();
	InputManager::getInstance().SetKeyButtonCallback([this](int key, int scancode, int action, int mods) {OnKeyButtonPressed(key, scancode, action, mods);}, 999);
}

void EngineStatus::ProcessWindow() {
	ProcessUnsavedChangesPopup();
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
		+ ButtonWidth("Save");
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
			if (FileManager::getInstance().currentProjectFile != "") {
				FileManager::getInstance().SaveProjectToFile(FileManager::getInstance().currentProjectFile);
			}
			EngineManager::getInstance().SaveEngineState();
		}
		EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
	}
	ImGui::SameLine();

	if (IconButton("pause", IconType::Pause, !isSimulating, iconDim)) {
		EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Pause);
	}
	ImGui::SameLine();

	if (IconButton("stop", IconType::Stop, isStopped, iconDim, stopRed)) {
		EngineManager::getInstance().LoadEngineState();
		EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Stop);
	}

	ImGui::SameLine(rightStartX);

	if (ImGui::Button("Settings"))
		ImGui::OpenPopup("Settings");
	ProcessSettingsPopup();

	ImGui::SameLine();

	bool saveDisabled = !isStopped || FileManager::getInstance().isSaved;
	ImGui::BeginDisabled(saveDisabled);
	if (ImGui::Button("Save")) {
		if (FileManager::getInstance().currentProjectFile.empty()) {
			auto opts = FileDialogOptions::ForExtension("Fusion Project", "fusion", "Save Project");
			opts.defaultFileName = "New Project.fusion";
			if (auto path = FileDialog::ShowSaveDialog(opts)) {
				FileManager::getInstance().currentProjectFile = *path;
				FileManager::getInstance().SaveProjectToFile(*path);
			}
		}
		else {
			FileManager::getInstance().SaveProjectToFile(FileManager::getInstance().currentProjectFile);
		}
	}

	ImGui::EndDisabled();

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
		bool dirty = !FileManager::getInstance().isSaved;
		const ImVec4 unsavedColor(0.95f, 0.65f, 0.25f, 1.0f); // amber
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
		ImGui::Checkbox("##Draw background grid", &settings.drawBackgroundGrid);

		ImGui::Text("Background color: ");
		ImGui::SameLine();
		ImGui::ColorEdit4("##Background color", &settings.backgroundColor.x);

		ImGui::Spacing();
		ImGui::SeparatorText("Debug");

		ImGui::Text("Draw object wire frame: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw object wire frame", &settings.drawObjectWireframe);

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

		ImGui::Text("Draw fluids as particles: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw fluids as particles", &settings.drawFluidsAsParticles);

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
			}
		}

		ImGui::Text("Draw velocity vector field: ");
		ImGui::SameLine();
		ImGui::Checkbox("##Draw velocity vector field", &settings.drawFluidsVelocityField);

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

void EngineStatus::ProcessUnsavedChangesPopup() {
	if (EngineManager::getInstance().pendingClose == true
		&& !ImGui::IsPopupOpen("Unsaved Changes"))
		ImGui::OpenPopup("Unsaved Changes");

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
			bool didSave = false;
			if (FileManager::getInstance().currentProjectFile.empty()) {
				auto opts = FileDialogOptions::ForExtension("Fusion Project", "fusion", "Save Project");
				opts.defaultFileName = "New Project.fusion";
				if (auto path = FileDialog::ShowSaveDialog(opts)) {
					FileManager::getInstance().currentProjectFile = *path;
					FileManager::getInstance().SaveProjectToFile(*path);
					didSave = true;
				}
			}
			else {
				FileManager::getInstance().SaveProjectToFile(FileManager::getInstance().currentProjectFile);
				didSave = true;
			}

			if (didSave) {
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
		if (FileManager::getInstance().currentProjectFile.empty()) {
			auto opts = FileDialogOptions::ForExtension("Fusion Project", "fusion", "Save Project");
			opts.defaultFileName = "New Project.fusion";
			if (auto path = FileDialog::ShowSaveDialog(opts)) {
				FileManager::getInstance().currentProjectFile = *path;
				FileManager::getInstance().SaveProjectToFile(*path);
			}
		}
		else {
			FileManager::getInstance().SaveProjectToFile(FileManager::getInstance().currentProjectFile);
		}
	}
}