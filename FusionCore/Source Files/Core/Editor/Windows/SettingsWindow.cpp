#include "../../../../Header Files/Core/Editor/Windows/SettingsWindow.h"
#include "../../../../Header Files/Core/Files/FileDialog.h"
#include "../../../../Header Files/Core/Editor/EditorField.h"
#include "../../../../Header Files/Core/Physics/PhysicsEngine.h"
#include <algorithm>
#include <cstdio>

SettingsWindow::SettingsWindow(std::string name) : EditorWindow(name) {}

void SettingsWindow::SetThemeStatus(const std::string& msg, bool isError) {
	themeStatus = msg;
	themeStatusIsError = isError;
}

void SettingsWindow::LoadThemeIntoEditor(const std::string& name) {
	ThemeManager& TM = ThemeManager::getInstance();
	ThemeManager::Theme t;
	std::string err;
	if (TM.SelectTheme(name, t, &err)) {
		editTheme = t;
		std::snprintf(themeNameBuf, sizeof(themeNameBuf), "%s", name.c_str());
		themeDirty = false;
		pendingApply = true;
		SetThemeStatus("", false);
	}
	else {
		SetThemeStatus(err, true);
	}
}

void SettingsWindow::ProcessWindow() {
	if (pendingApply) {
		EditorTheme::currentTheme = editTheme;
		EditorTheme::Apply();
		pendingApply = false;
	}

	if (openRequested) {
		openRequested = false;
		isOpen = true;
		if (!editorSynced) {
			editTheme = EditorTheme::currentTheme;
			std::snprintf(themeNameBuf, sizeof(themeNameBuf), "%s",
				ThemeManager::getInstance().GetActiveName().c_str());
			editorSynced = true;
		}
		ImGui::SetNextWindowFocus();
	}

	if (!isOpen || hidden) return;

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_Appearing);

	if (!ImGui::Begin("Settings", &isOpen,
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	static const char* categories[] = { "General", "Physics", "Viewport", "Debug Draw", "Themes" };
	const float footerH = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f + 6.0f;

	ImGui::BeginChild("##SettingsCategories", ImVec2(160, -footerH), ImGuiChildFlags_Borders);
	ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
	for (int i = 0; i < IM_ARRAYSIZE(categories); i++) {
		if (ImGui::Selectable(categories[i], category == i, 0, ImVec2(0, ImGui::GetFrameHeight() + 4.0f)))
			category = i;
	}
	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##SettingsContent", ImVec2(0, -footerH));

	bool boldPushed = EditorTheme::PushBold();
	ImGui::TextUnformatted(categories[category]);
	EditorTheme::PopBold(boldPushed);
	ImGui::Separator();
	ImGui::Spacing();

	switch (category) {
	case 0: DrawGeneral(); break;
	case 1: DrawPhysics(); break;
	case 2: DrawViewport(); break;
	case 3: DrawDebugDraw(); break;
	case 4: DrawThemes(); break;
	}

	ImGui::EndChild();

	ImGui::Separator();
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120.0f - style.WindowPadding.x);
	if (ImGui::Button("Close", ImVec2(120, 0))) isOpen = false;

	ImGui::End();
}

void SettingsWindow::DrawGeneral() {
	EngineManager& EM = EngineManager::getInstance();
	Settings& settings = EM.EngineSettings;
	const ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SeparatorText("Project");

	EditorField::Detail::Label("Main scene");
	{
		char sceneBuf[256];
#if defined(_MSC_VER)
		strcpy_s(sceneBuf, settings.mainScenePath.c_str());
#else
		strncpy(sceneBuf, settings.mainScenePath.c_str(), sizeof(sceneBuf) - 1);
		sceneBuf[sizeof(sceneBuf) - 1] = '\0';
#endif
		const float browseW = ImGui::CalcTextSize("Browse").x + style.FramePadding.x * 2.0f;
		ImGui::SetNextItemWidth(-(browseW + style.ItemSpacing.x));
		ImGui::InputTextWithHint("##MainScene", "None (plays current scene)", sceneBuf,
			IM_ARRAYSIZE(sceneBuf), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse##MainScene")) {
			auto opts = FileDialogOptions::ForExtension("Fusion Scene", "fscene", "Choose Main Scene");
			if (auto path = FileDialog::ShowOpenDialog(opts)) {
				settings.mainScenePath = *path;
				EM.EngineChangeEvent();
			}
		}
	}

	ImGui::Spacing();
	ImGui::SeparatorText("Game View");

	int res[2] = { (int)EM.resolutionWidth, (int)EM.resolutionHeight };
	EditorField::InputInt2Engine("Game resolution", "##GameResolution", res, [&] {
		EM.SetGameResolution((float)std::max(1, res[0]), (float)std::max(1, res[1]));
		});
}

void SettingsWindow::DrawPhysics() {
	EngineManager& EM = EngineManager::getInstance();
	Settings& settings = EM.EngineSettings;
	PhysicsSettings& p = settings.physics;

	auto F = [&](const char* label, const char* id, float& v, float speed, float mn, float mx,
		const char* fmt, const char* tip = nullptr) {
			EditorField::Detail::Label(label);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat(id, &v, speed, mn, mx, fmt, ImGuiSliderFlags_AlwaysClamp);
			if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
			if (ImGui::IsItemDeactivatedAfterEdit()) EM.EngineChangeEvent();
		};
	auto I = [&](const char* label, const char* id, int& v, int mn, int mx, const char* tip = nullptr) {
		EditorField::Detail::Label(label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragInt(id, &v, 0.2f, mn, mx, "%d", ImGuiSliderFlags_AlwaysClamp);
		if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
		if (ImGui::IsItemDeactivatedAfterEdit()) EM.EngineChangeEvent();
		};

	if (ImGui::Button("Reset all physics to defaults")) {
		p = PhysicsSettings();
		EM.EngineChangeEvent();
	}
	ImGui::Spacing();

	ImGui::SeparatorText("Collision");
	const char* modeLabels[] = { "AABB", "Bounding Circle" };
	int current = static_cast<int>(settings.broadPhaseMode);
	EditorField::ComboEngine("Broad phase mode", "##BroadPhaseMode", &current, modeLabels,
		IM_ARRAYSIZE(modeLabels), [&] {
			PhysicsEngine::getInstance().SetBroadPhaseMode(static_cast<BroadPhaseMode>(current));
		});
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"AABB is more efficient for wide or tall objects.\n"
			"Bounding Circle can be cheaper/tighter for roughly round objects.\n"
			"Switching rebuilds the broad phase for every collidable object.");
	}

	ImGui::Spacing();
	ImGui::SeparatorText("Interaction");
	EditorField::CheckboxEngine("Physics interact", "##PhysicsInteract", &settings.physicsInteract);
	F("Drag stiffness", "##DragK", p.dragStiffness, 1.0f, 0.0f, 10000.0f, "%.1f", "Mouse drag spring constant (scaled by mass).");
	F("Drag damping", "##DragC", p.dragDamping, 0.1f, 0.0f, 1000.0f, "%.2f", "Mouse drag damping (scaled by mass).");

	ImGui::Spacing();
	ImGui::SeparatorText("World");
	{
		EditorField::Detail::Label("Gravity");
		ImGui::SetNextItemWidth(-FLT_MIN);
		float g[2] = { p.gravity.x, p.gravity.y };
		if (ImGui::DragFloat2("##Gravity", g, 0.05f, -100.0f, 100.0f, "%.2f m/s²")) {
			p.gravity.x = g[0]; p.gravity.y = g[1];
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) EM.EngineChangeEvent();
	}

	if (ImGui::CollapsingHeader("Rigid Body Solver (PGS)")) {
		I("Iterations (frame)", "##PgsIter", p.pgsFrameIterations, 1, 200, "Velocity iterations for rigid contacts each frame.");
		I("Iterations (substep)", "##PgsSubIter", p.pgsSubstepIterations, 1, 200, "Iterations for constraints solved inside soft body substeps.");
		F("Contact bias factor", "##CBeta", p.contactBeta, 0.005f, 0.0f, 1.0f, "%.3f", "Baumgarte-style penetration recovery. Higher = faster push-out, more jitter.");
		F("Static friction", "##CSF", p.contactStaticFriction, 0.01f, 0.0f, 5.0f, "%.3f");
		F("Dynamic friction", "##CDF", p.contactDynamicFriction, 0.01f, 0.0f, 5.0f, "%.3f");
		F("Position correction", "##RPC", p.rigidPositionCorrection, 0.005f, 0.0f, 1.0f, "%.3f", "Fraction of penetration removed by direct position projection.");
		F("Slop", "##RSlop", p.rigidSlop, 0.0005f, 0.0f, 0.1f, "%.4f", "Penetration tolerated before correction.");
		F("Sleep linear threshold", "##SleepL", p.rigidSleepLinear, 0.0001f, 0.0f, 1.0f, "%.4f m/s", "Below this speed velocity snaps to zero.");
		F("Sleep angular threshold", "##SleepA", p.rigidSleepAngular, 0.0001f, 0.0f, 1.0f, "%.4f rad/s");
	}

	if (ImGui::CollapsingHeader("Soft Body (XPBD)")) {
		I("Substeps", "##XSub", p.xpbdSubsteps, 1, 64);
		I("Position iterations", "##XIter", p.xpbdPositionIterations, 1, 64);
		F("Area compliance", "##SAC", p.softAreaCompliance, 0.0001f, 0.0f, 10.0f, "%.5f", "Applied when a soft body is rebuilt (Reset shape / restart).");
		F("Triangle area compliance", "##STAC", p.softTriAreaCompliance, 0.0001f, 0.0f, 10.0f, "%.5f", "Applied when a soft body is rebuilt (Reset shape / restart).");
		F("Gas max force / edge", "##GasMax", p.softGasMaxForcePerEdge, 0.5f, 0.0f, 10000.0f, "%.1f");
		F("Soft-soft compliance", "##SSC", p.softSoftCompliance, 0.00001f, 0.0f, 1.0f, "%.6f");
		F("Soft-soft friction A", "##SSFA", p.softSoftStaticFriction, 0.01f, 0.0f, 5.0f, "%.3f");
		F("Soft-soft friction B", "##SSFB", p.softSoftDynamicFriction, 0.01f, 0.0f, 5.0f, "%.3f");
	}

	if (ImGui::CollapsingHeader("Soft <-> Rigid Contacts")) {
		I("Iterations", "##SRIter", p.srContactIterations, 1, 64);
		F("Bias factor", "##SRBeta", p.srBeta, 0.005f, 0.0f, 1.0f, "%.3f");
		F("Slop", "##SRSlop", p.srSlop, 0.0001f, 0.0f, 0.1f, "%.4f");
		F("Max bias velocity", "##SRMaxB", p.srMaxBiasVelocity, 0.05f, 0.0f, 100.0f, "%.2f");
		F("Restitution (soft vs rigid)", "##SRRest", p.softRigidRestitution, 0.01f, 0.0f, 1.0f, "%.3f");
		F("Restitution (rigid vs soft)", "##RSRest", p.rigidSoftRestitution, 0.01f, 0.0f, 1.0f, "%.3f");
		F("Static friction", "##SRSF", p.srStaticFriction, 0.01f, 0.0f, 5.0f, "%.3f");
		F("Dynamic friction", "##SRDF", p.srDynamicFriction, 0.01f, 0.0f, 5.0f, "%.3f");
		F("Position correction", "##SRPC", p.srPositionCorrection, 0.005f, 0.0f, 1.0f, "%.3f");
		F("Rigid vertex radius", "##RVR", p.rigidVertexRadius, 0.001f, 0.0f, 1.0f, "%.4f");
	}

	if (ImGui::CollapsingHeader("Fluid & Gas (PBF)")) {
		I("Substeps", "##PbfSub", p.pbfSubsteps, 1, 32);
		I("Solver iterations", "##PbfIter", p.pbfSolveIterations, 1, 32);
		F("Tension correction (k)", "##SCorrK", p.pbfSCorrK, 0.005f, 0.0f, 5.0f, "%.3f", "Artificial pressure that prevents particle clumping.");
		F("Tension correction dq", "##SCorrDQ", p.pbfSCorrDeltaQ, 0.005f, 0.01f, 1.0f, "%.3f", "Fraction of smoothing radius.");
		F("Gas min temperature", "##GasMinT", p.gasMinTemperature, 0.1f, 0.01f, 10000.0f, "%.2f");
	}

	if (ImGui::CollapsingHeader("Fluid Boundary Contacts")) {
		I("Iterations", "##FCIter", p.fluidContactIterations, 1, 64);
		F("Bias factor", "##FCBeta", p.fluidContactBeta, 0.005f, 0.0f, 1.0f, "%.3f");
		F("Slop", "##FCSlop", p.fluidContactSlop, 0.0001f, 0.0f, 0.1f, "%.4f");
		F("Restitution", "##FCRest", p.fluidContactRestitution, 0.01f, 0.0f, 1.0f, "%.3f");
		F("Max bias velocity", "##FCMaxB", p.fluidMaxBiasVelocity, 0.05f, 0.0f, 100.0f, "%.2f");
		F("Position correction", "##FCPC", p.fluidPositionCorrection, 0.005f, 0.0f, 1.0f, "%.3f");
		F("Penetration clamp", "##FCClamp", p.fluidPenetrationClamp, 0.05f, 0.1f, 20.0f, "%.2f x radius");
	}

	if (ImGui::CollapsingHeader("Buoyancy")) {
		F("Linear drag", "##BLD", p.buoyancyLinearDrag, 0.1f, 0.0f, 500.0f, "%.2f");
		F("Angular drag", "##BAD", p.buoyancyAngularDrag, 0.01f, 0.0f, 100.0f, "%.3f");
		I("Surface min neighbours", "##BMN", p.buoyancyMinNeighbours, 1, 64, "Particles need this many neighbours to count as the fluid surface.");
	}

	if (ImGui::CollapsingHeader("Fracture")) {
		F("Seed min distance", "##FSD", p.fractureSeedMinDistance, 0.005f, 0.0f, 1.0f, "%.3f", "Fraction of the object's bounding-box diagonal.");
		F("Min shard mass", "##FMM", p.fractureMinShardMass, 0.0005f, 0.00001f, 10.0f, "%.4f kg");
	}
}

void SettingsWindow::DrawViewport() {
	Settings& settings = EngineManager::getInstance().EngineSettings;

	ImGui::SeparatorText("Appearance");
	EditorField::CheckboxEngine("Background grid", "##DrawGrid", &settings.drawBackgroundGrid);
	EditorField::ColorEdit4Engine("Background color", "##BackgroundColor", &settings.backgroundColor.x);
}

void SettingsWindow::DrawDebugDraw() {
	Settings& settings = EngineManager::getInstance().EngineSettings;

	ImGui::SeparatorText("Objects");
	EditorField::CheckboxEngine("Object wireframe", "##DrawWireframe", &settings.drawObjectWireframe);

	ImGui::Spacing();
	ImGui::SeparatorText("Collision");
	EditorField::CheckboxEngine("Broad phase bounds", "##DrawBroadPhase", &settings.drawBroadPhaseBounds);
	EditorField::CheckboxEngine("Collision shapes", "##DrawCollisionShapes", &settings.drawCollisionShapes);
	EditorField::CheckboxEngine("Collision normals", "##DrawCollisionNormals", &settings.drawCollisionNormals);
	EditorField::CheckboxEngine("Contact points", "##DrawContactPoints", &settings.drawContactPoints);

	ImGui::Spacing();
	ImGui::SeparatorText("Soft Bodies");
	EditorField::CheckboxEngine("Point masses", "##DrawPointMasses", &settings.drawSoftBodyPointMasses);
	EditorField::CheckboxEngine("Springs", "##DrawSprings", &settings.drawSoftBodySprings);
	EditorField::CheckboxEngine("Virtual proxies", "##DrawProxies", &settings.drawVirtualSoftBodyProxies);

	ImGui::Spacing();
	ImGui::SeparatorText("Fluids & Gas");
	EditorField::CheckboxEngine("Draw as particles", "##DrawFluidGasParticles", &settings.drawFluidsAsParticles);

	ImGui::BeginDisabled(!settings.drawFluidsAsParticles);
	const char* heatmapLabels[] = { "None", "Velocity", "Density" };
	int heat = static_cast<int>(settings.fluidHeatmapMode);
	EditorField::ComboEngine("Heatmap", "##FluidGasHeatmap", &heat, heatmapLabels,
		IM_ARRAYSIZE(heatmapLabels), [&] {
			settings.fluidHeatmapMode = static_cast<FluidHeatmapMode>(heat);
		});
	EditorField::CheckboxEngine("Velocity vector field", "##DrawFluidGasVelocity", &settings.drawFluidsVelocityField);
	ImGui::EndDisabled();
}

void SettingsWindow::DrawThemes() {
	ThemeManager& TM = ThemeManager::getInstance();
	const std::string active = TM.GetActiveName();
	const bool isDefault = ThemeManager::IsDefault(active);

	const std::string cleanName = ThemeManager::SanitizeName(themeNameBuf);
	const bool nameChanged = !isDefault && cleanName != active;
	const bool unsaved = themeDirty || nameChanged;
	const std::string targetName = isDefault ? active : cleanName;

	const ImVec4 unsavedCol(0.95f, 0.65f, 0.25f, 1.0f);

	ImGui::SeparatorText("Theme");

	EditorField::Detail::Label("Active theme");
	ImGui::SetNextItemWidth(-FLT_MIN);
	const std::string preview = active + (unsaved ? "  *" : "");
	if (unsaved) ImGui::PushStyleColor(ImGuiCol_Text, unsavedCol);
	const bool comboOpen = ImGui::BeginCombo("##ThemeSelect", preview.c_str());
	if (unsaved) ImGui::PopStyleColor();   
	if (comboOpen) {
		for (const std::string& n : TM.GetThemeNames()) {
			const bool selected = (n == active);
			if (ImGui::Selectable(n.c_str(), selected)) LoadThemeIntoEditor(n);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	EditorField::Detail::Label("Name");
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::BeginDisabled(isDefault);
	if (nameChanged) ImGui::PushStyleColor(ImGuiCol_Text, unsavedCol);
	ImGui::InputText("##ThemeName", themeNameBuf, IM_ARRAYSIZE(themeNameBuf));
	if (nameChanged) ImGui::PopStyleColor();
	ImGui::EndDisabled();

	const bool canSave = unsaved && !targetName.empty();
	ImGui::BeginDisabled(!canSave);
	if (unsaved) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.38f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.49f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.31f, 0.08f, 1.0f));
	}
	const bool savePressed = ImGui::Button("Save", ImVec2(90, 0));
	if (unsaved) ImGui::PopStyleColor(3);
	ImGui::EndDisabled();

	if (savePressed) {
		std::string err;
		bool ok = true;
		if (!isDefault && targetName != active)
			ok = TM.RenameTheme(active, targetName, &err);
		if (ok) {
			editTheme.name = targetName;
			ok = TM.SaveTheme(targetName, editTheme, &err);
		}
		if (ok) {
			std::snprintf(themeNameBuf, sizeof(themeNameBuf), "%s", targetName.c_str());
			themeDirty = false;
			SetThemeStatus("Saved to Resources/Themes/" + targetName + ".json", false);
		}
		else {
			SetThemeStatus(err, true);
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!unsaved);
	if (ImGui::Button("Revert", ImVec2(90, 0))) LoadThemeIntoEditor(active);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Discard unsaved changes and reload the saved theme.");

	ImGui::SameLine();
	if (EditorField::AddButton("New Theme...")) {
		newThemeNameBuf[0] = '\0';
		ImGui::OpenPopup("New Theme");
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Create a new theme from the colors currently in the editor.");

	if (ImGui::Button("Import...", ImVec2(90, 0))) {
		auto opts = FileDialogOptions::ForExtension("Fusion Theme", "json", "Import Theme");
		if (auto path = FileDialog::ShowOpenDialog(opts)) {
			std::string importedName, err;
			if (TM.ImportTheme(*path, &importedName, &err)) {
				LoadThemeIntoEditor(importedName);
				SetThemeStatus("Imported \"" + importedName + "\".", false);
			}
			else {
				SetThemeStatus(err, true);
			}
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(unsaved);
	if (ImGui::Button("Export...", ImVec2(90, 0))) {
		if (auto folder = FileDialog::ShowFolderDialog("Export Theme To Folder")) {
			std::string err;
			if (TM.ExportTheme(active, *folder, &err))
				SetThemeStatus("Exported \"" + active + ".json\" - send that file to anyone, they can Import it.", false);
			else
				SetThemeStatus(err, true);
		}
	}
	ImGui::EndDisabled();
	if (unsaved && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Save your changes before exporting.");

	ImGui::SameLine();
	if (isDefault) {
		if (EditorField::DangerButton("Reset to Original", ImVec2(140, 0)))
			ImGui::OpenPopup("Reset Default Theme?");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Restore the Default theme to the engine's original colors.");
	}
	else {
		if (EditorField::DangerButton("Delete", ImVec2(90, 0)))
			ImGui::OpenPopup("Delete Theme?");
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	const ImGuiWindowFlags popupFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("New Theme", nullptr, popupFlags)) {
		ImGui::TextUnformatted("Name for the new theme");
		ImGui::SetNextItemWidth(280.0f);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::InputText("##NewThemeName", newThemeNameBuf, IM_ARRAYSIZE(newThemeNameBuf));
		ImGui::TextDisabled("Starts as a copy of the colors currently in the editor.");
		ImGui::Spacing();

		const std::string newName = ThemeManager::SanitizeName(newThemeNameBuf);
		const bool exists = !newName.empty() && TM.ThemeExists(newName);
		if (exists) {
			ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Danger());
			ImGui::TextUnformatted("A theme with that name already exists.");
			ImGui::PopStyleColor();
		}

		ImGui::BeginDisabled(newName.empty() || exists);
		if (EditorField::AddButton("Create")) {
			std::string err;
			editTheme.name = newName;
			if (TM.CreateTheme(newName, editTheme, &err)) {
				std::snprintf(themeNameBuf, sizeof(themeNameBuf), "%s", newName.c_str());
				themeDirty = false;
				SetThemeStatus("Created \"" + newName + "\".", false);
			}
			else {
				SetThemeStatus(err, true);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(90, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Delete Theme?", nullptr, popupFlags)) {
		ImGui::Text("Delete theme \"%s\"? This removes its file.", active.c_str());
		ImGui::Spacing();
		if (EditorField::DangerButton("Delete", ImVec2(120, 0))) {
			std::string err;
			if (TM.DeleteTheme(active, &err)) {
				LoadThemeIntoEditor("Default");
				SetThemeStatus("Theme deleted.", false);
			}
			else {
				SetThemeStatus(err, true);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Reset Default Theme?", nullptr, popupFlags)) {
		ImGui::TextUnformatted("Restore the Default theme to the engine's original colors?");
		ImGui::TextDisabled("Your edits to Default will be lost.");
		ImGui::Spacing();
		if (EditorField::DangerButton("Reset", ImVec2(120, 0))) {
			ThemeManager::Theme original;
			std::string err;
			if (TM.ResetDefault(original, &err)) {
				LoadThemeIntoEditor("Default");
				SetThemeStatus("Default theme restored to original.", false);
			}
			else {
				SetThemeStatus(err, true);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (unsaved) {
		ImGui::TextColored(unsavedCol, "* Unsaved changes - press Save to keep them.");
	}
	else if (!themeStatus.empty()) {
		const ImVec4 col = themeStatusIsError ? EditorTheme::Danger() : EditorTheme::Accent();
		ImGui::PushStyleColor(ImGuiCol_Text, col);
		ImGui::TextWrapped("%s", themeStatus.c_str());
		ImGui::PopStyleColor();
	}
	if (unsaved && themeStatusIsError && !themeStatus.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Danger());
		ImGui::TextWrapped("%s", themeStatus.c_str());
		ImGui::PopStyleColor();
	}

	const char* lastGroup = nullptr;
	for (const auto& e : ThemeManager::Entries()) {
		if (!lastGroup || std::strcmp(lastGroup, e.group) != 0) {
			ImGui::Spacing();
			ImGui::SeparatorText(e.group);
			lastGroup = e.group;
		}

		ImVec4& color = editTheme.*(e.member);
		std::string id = std::string("##theme_") + e.key;

		EditorField::Detail::Label(e.label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::ColorEdit4(id.c_str(), &color.x, ImGuiColorEditFlags_NoAlpha)) {
			themeDirty = true;
			pendingApply = true;   
		}
	}
}