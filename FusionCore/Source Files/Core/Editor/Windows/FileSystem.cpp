#include "../../../../Header Files/Core/Editor/Windows/FileSystem.h"
#include "../../../../Header Files/Core/Files/FileDialog.h"
#include "../../../../Header Files/Core/SceneManager.h"
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
	std::string ToLower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	void DrawFolderIcon(ImDrawList* dl, ImVec2 pos, float size, ImU32 color) {
		float tabW = size * 0.5f;
		float tabH = size * 0.18f;
		dl->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + tabW, pos.y + tabH), color, 2.0f);
		dl->AddRectFilled(ImVec2(pos.x, pos.y + tabH * 0.6f), ImVec2(pos.x + size, pos.y + size), color, 2.5f);
	}

	void DrawFileIcon(ImDrawList* dl, ImVec2 pos, float size, ImU32 color) {
		float bodyW = size * 0.78f;
		float foldSize = size * 0.26f;
		dl->AddRectFilled(pos, ImVec2(pos.x + bodyW, pos.y + size), color, 2.0f);
		dl->AddTriangleFilled(
			ImVec2(pos.x + bodyW - foldSize, pos.y),
			ImVec2(pos.x + bodyW, pos.y),
			ImVec2(pos.x + bodyW, pos.y + foldSize),
			IM_COL32(25, 25, 25, 255));
	}


	void DrawScriptIcon(ImDrawList* dl, ImVec2 pos, float size, ImU32 bodyColor, ImU32 glyphColor) {
		DrawFileIcon(dl, pos, size, bodyColor);

		float bodyW = size * 0.78f;
		float cx = pos.x + bodyW * 0.5f;
		float cy = pos.y + size * 0.62f;
		float halfW = bodyW * 0.22f;
		float halfH = size * 0.14f;
		float thickness = size * 0.07f;
		if (thickness < 1.0f) thickness = 1.0f;

		dl->AddLine(ImVec2(cx - halfW * 0.2f, cy - halfH), ImVec2(cx - halfW, cy), glyphColor, thickness);
		dl->AddLine(ImVec2(cx - halfW, cy), ImVec2(cx - halfW * 0.2f, cy + halfH), glyphColor, thickness);

		dl->AddLine(ImVec2(cx + halfW * 0.2f, cy - halfH), ImVec2(cx + halfW, cy), glyphColor, thickness);
		dl->AddLine(ImVec2(cx + halfW, cy), ImVec2(cx + halfW * 0.2f, cy + halfH), glyphColor, thickness);
	}

	void DrawSceneIcon(ImDrawList* dl, ImVec2 pos, float size, ImU32 bodyColor, ImU32 glyphColor) {
		DrawFileIcon(dl, pos, size, bodyColor);

		float bodyW = size * 0.78f;
		float baseY = pos.y + size * 0.72f;
		float leftX = pos.x + bodyW * 0.14f;
		float rightX = pos.x + bodyW * 0.86f;

		ImVec2 sunCenter(pos.x + bodyW * 0.68f, pos.y + size * 0.32f);
		dl->AddCircleFilled(sunCenter, size * 0.09f, glyphColor);

		ImVec2 peak1(pos.x + bodyW * 0.30f, pos.y + size * 0.40f);
		ImVec2 peak2(pos.x + bodyW * 0.55f, pos.y + size * 0.52f);

		dl->AddTriangleFilled(ImVec2(leftX, baseY), peak1, ImVec2(pos.x + bodyW * 0.48f, baseY), glyphColor);
		dl->AddTriangleFilled(ImVec2(pos.x + bodyW * 0.36f, baseY), peak2, ImVec2(rightX, baseY), glyphColor);
	}

	void DrawArchiveIcon(ImDrawList* dl, ImVec2 pos, float size, ImU32 bodyColor, ImU32 glyphColor) {
		DrawFileIcon(dl, pos, size, bodyColor);

		float bodyW = size * 0.78f;
		float cx = pos.x + bodyW * 0.5f;

		float zipTop = pos.y + size * 0.14f;
		float zipBottom = pos.y + size * 0.92f;
		float zipW = size * 0.14f;

		dl->AddLine(ImVec2(cx, zipTop), ImVec2(cx, zipBottom), glyphColor, std::max(1.0f, size * 0.05f));

		int teeth = 4;
		float step = (zipBottom - zipTop) / (teeth * 2.0f);
		for (int i = 0; i < teeth; i++) {
			float y = zipTop + step * (i * 2 + 1);
			bool left = (i % 2) == 0;
			float x0 = left ? cx - zipW : cx;
			float x1 = left ? cx : cx + zipW;
			dl->AddRectFilled(ImVec2(x0, y - step * 0.35f), ImVec2(x1, y + step * 0.35f), glyphColor, 1.0f);
		}
	}

	void OpenPathInVSCode(const std::filesystem::path& projectDir, const std::filesystem::path& fileAbsPath) {
		std::string args = "\"" + projectDir.string() + "\" -g \"" + fileAbsPath.string() + "\"";

#ifdef _WIN32
		ShellExecuteA(nullptr, "open", "code", args.c_str(), nullptr, SW_HIDE);
#else
		std::string command = "code " + args + " >/dev/null 2>&1 &";
		std::system(command.c_str());
#endif
	}
}

FileSystem::FileSystem(std::string name) : EditorWindow(name) {
	expandedPaths.insert(FileManager::getInstance().GetRootVirtualPath());
}

void FileSystem::SelectPath(const std::string& virtualPath) {
	selectedPath = virtualPath;

	strncpy_s(pathDisplayBuf, virtualPath.c_str(), sizeof(pathDisplayBuf) - 1);
	pathDisplayBuf[sizeof(pathDisplayBuf) - 1] = '\0';

	if (!navigatingHistory) {
		if (historyIndex < (int)history.size() - 1)
			history.resize(historyIndex + 1);
		if (history.empty() || history.back() != virtualPath) {
			history.push_back(virtualPath);
			historyIndex = (int)history.size() - 1;
		}
	}
}

void FileSystem::NavigateBack() {
	if (historyIndex <= 0) return;
	historyIndex--;

	navigatingHistory = true;
	SelectPath(history[historyIndex]);
	navigatingHistory = false;

	ExpandParentsOf(history[historyIndex]);
	pendingScrollTarget = history[historyIndex];
}

void FileSystem::NavigateForward() {
	if (historyIndex >= (int)history.size() - 1) return;
	historyIndex++;

	navigatingHistory = true;
	SelectPath(history[historyIndex]);
	navigatingHistory = false;

	ExpandParentsOf(history[historyIndex]);
	pendingScrollTarget = history[historyIndex];
}

void FileSystem::ExpandParentsOf(const std::string& virtualPath) {
	FileManager& fm = FileManager::getInstance();

	std::filesystem::path root = fm.GetResourcesRoot();
	std::filesystem::path current = fm.VirtualToAbsolute(virtualPath).parent_path();

	while (true) {
		expandedPaths.insert(fm.AbsoluteToVirtual(current));
		if (current == root) break;
		std::filesystem::path parent = current.parent_path();
		if (parent == current) break; 
		current = parent;
	}

	expandedPaths.insert(fm.GetRootVirtualPath());
}

std::string FileSystem::GetParentVirtualPath(const std::string& virtualPath) const {
	FileManager& fm = FileManager::getInstance();
	return fm.AbsoluteToVirtual(fm.VirtualToAbsolute(virtualPath).parent_path());
}

bool FileSystem::SubtreeMatchesFilter(const std::string& virtualPath, const std::string& filterLower) const {
	FileManager& fm = FileManager::getInstance();

	std::string name = ToLower(fm.VirtualToAbsolute(virtualPath).filename().string());
	if (name.find(filterLower) != std::string::npos)
		return true;

	if (!fm.IsDirectory(virtualPath))
		return false;

	for (auto& child : fm.GetDirectoryContents(virtualPath)) {
		if (SubtreeMatchesFilter(child.virtualPath, filterLower))
			return true;
	}
	return false;
}


void FileSystem::BeginRename(const std::string& virtualPath, const std::string& currentName) {
	if (virtualPath == FileManager::getInstance().GetRootVirtualPath())
		return;

	renamingPath = virtualPath;
	strncpy_s(renameBuf, currentName.c_str(), sizeof(renameBuf) - 1);
	renameBuf[sizeof(renameBuf) - 1] = '\0';
	renameJustOpened = true;
}

void FileSystem::CommitRename() {
	std::string oldPath = renamingPath;
	std::string newName = renameBuf;
	renamingPath.clear();

	if (oldPath.empty() || newName.empty())
		return;

	FileManager& fm = FileManager::getInstance();
	if (!fm.RenameResource(oldPath, newName))
		return;

	std::filesystem::path newAbs = fm.VirtualToAbsolute(oldPath).parent_path() / newName;
	std::string newVirtualPath = fm.AbsoluteToVirtual(newAbs);

	if (expandedPaths.count(oldPath)) {
		expandedPaths.erase(oldPath);
		expandedPaths.insert(newVirtualPath);
	}
	if (selectedPath == oldPath) {
		SelectPath(newVirtualPath);
	}
}

void FileSystem::CancelRename() {
	renamingPath.clear();
}

void FileSystem::MoveEntry(const std::string& sourceVirtualPath, const std::string& destDirVirtualPath) {
	FileManager& fm = FileManager::getInstance();

	std::filesystem::path sourceAbs = fm.VirtualToAbsolute(sourceVirtualPath);
	std::string destVirtualPath = fm.AbsoluteToVirtual(fm.VirtualToAbsolute(destDirVirtualPath) / sourceAbs.filename());

	if (!fm.MoveResource(sourceVirtualPath, destDirVirtualPath))
		return;

	if (expandedPaths.count(sourceVirtualPath)) {
		expandedPaths.erase(sourceVirtualPath);
		expandedPaths.insert(destVirtualPath);
	}
	if (selectedPath == sourceVirtualPath) {
		SelectPath(destVirtualPath);
	}
	if (renamingPath == sourceVirtualPath) {
		renamingPath.clear();
	}

	expandedPaths.insert(destDirVirtualPath);
}

void FileSystem::AddFileToFolder(const std::string& folderVirtualPath) {
	FileDialogOptions options;
	options.title = "Add File";

	std::vector<std::string> picked = FileDialog::ShowOpenDialogMulti(options);
	if (picked.empty())
		return;

	FileManager& fm = FileManager::getInstance();
	for (auto& sourcePath : picked)
		fm.ImportFile(sourcePath, folderVirtualPath);

	expandedPaths.insert(folderVirtualPath);
}

void FileSystem::DrawNode(const FileSystemEntry& entry, int depth) {
	ImGui::PushID(entry.virtualPath.c_str());

	std::string filterLower = ToLower(filterBuf);
	bool filterActive = !filterLower.empty();

	bool isExpanded = entry.isDirectory && (filterActive || expandedPaths.count(entry.virtualPath) > 0);
	bool isSelected = (selectedPath == entry.virtualPath);
	bool isRenaming = (renamingPath == entry.virtualPath);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding;
	if (isSelected)
		flags |= ImGuiTreeNodeFlags_Selected;
	if (!entry.isDirectory)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	if (entry.isDirectory)
		ImGui::SetNextItemOpen(isExpanded, ImGuiCond_Always);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 4.0f));
	bool nodeOpen = ImGui::TreeNodeEx("##node", flags);
	ImGui::PopStyleVar();

	bool toggledOpen = entry.isDirectory && ImGui::IsItemToggledOpen();
	if (toggledOpen) {
		if (nodeOpen) expandedPaths.insert(entry.virtualPath);
		else expandedPaths.erase(entry.virtualPath);
	}

	bool rowClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	bool rowDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	if (rowClicked && !isRenaming && !toggledOpen) {
		SelectPath(entry.virtualPath);
	}
	if (rowDoubleClicked && !isRenaming && !toggledOpen && entry.iconType == ResourceIconType::Folder) {
		BeginRename(entry.virtualPath, entry.name);
	}
	if (rowDoubleClicked && !isRenaming && !toggledOpen && entry.iconType == ResourceIconType::Script) {
		OpenPathInVSCode(FileManager::getInstance().currentProjectDirectory, entry.absolutePath);
	}
	if (rowDoubleClicked && !isRenaming && !toggledOpen && entry.iconType == ResourceIconType::Scene
		&& EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Stop) { 
		SceneManager::getInstance().OpenSceneTab(entry.absolutePath.string());
	}

	bool isRoot = entry.virtualPath == FileManager::getInstance().GetRootVirtualPath();
	if (!isRenaming && !isRoot && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
		ImGui::SetDragDropPayload(FileManager::kResourceDragDropPayloadType,
			entry.virtualPath.c_str(), entry.virtualPath.size() + 1);
		ImGui::TextUnformatted(entry.name.c_str());
		ImGui::EndDragDropSource();
	}

	if (entry.isDirectory && ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FileManager::kResourceDragDropPayloadType)) {
			std::string draggedPath(static_cast<const char*>(payload->Data));
			MoveEntry(draggedPath, entry.virtualPath);
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginPopupContextItem("##ctx")) {
		ProcessEntryContextMenu(entry);
		ImGui::EndPopup();
	}

	if (!pendingScrollTarget.empty() && pendingScrollTarget == entry.virtualPath) {
		ImGui::SetScrollHereY(0.5f);
		pendingScrollTarget.clear();
	}

	ImVec2 rowMin = ImGui::GetItemRectMin();
	ImVec2 rowMax = ImGui::GetItemRectMax();
	float rowHeight = rowMax.y - rowMin.y;
	float labelOffset = ImGui::GetTreeNodeToLabelSpacing();
	float iconSize = rowHeight - 6.0f;
	ImVec2 iconPos(rowMin.x + labelOffset, rowMin.y + (rowHeight - iconSize) * 0.5f);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	if (entry.isDirectory) {
		DrawFolderIcon(dl, iconPos, iconSize, IM_COL32(235, 200, 100, 255));
	}
	else if (entry.iconType == ResourceIconType::Image) {
		unsigned int tex = FileManager::getInstance().GetOrLoadThumbnail(entry.absolutePath);
		if (tex != 0)
			dl->AddImage((ImTextureID)(intptr_t)tex, iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
		else
			DrawFileIcon(dl, iconPos, iconSize, IM_COL32(120, 170, 230, 255));
	}
	else if (entry.iconType == ResourceIconType::Script) {
		DrawScriptIcon(dl, iconPos, iconSize, IM_COL32(90, 160, 110, 255), IM_COL32(235, 235, 235, 255));
	}
	else if (entry.iconType == ResourceIconType::Scene) {                             
		DrawSceneIcon(dl, iconPos, iconSize, IM_COL32(150, 110, 200, 255), IM_COL32(235, 235, 235, 255));
	}
	else if (entry.iconType == ResourceIconType::Archive) {
		DrawArchiveIcon(dl, iconPos, iconSize, IM_COL32(200, 170, 90, 255), IM_COL32(35, 30, 15, 255));
	}
	else {
		DrawFileIcon(dl, iconPos, iconSize, IM_COL32(160, 160, 160, 255));
	}

	float textX = iconPos.x + iconSize + 6.0f;

	if (isRenaming) {
		ImGui::SetCursorScreenPos(ImVec2(textX, rowMin.y));
		ImGui::SetNextItemWidth(rowMax.x - textX - 4.0f);

		if (renameJustOpened) {
			ImGui::SetKeyboardFocusHere();
			renameJustOpened = false;
		}

		bool submitted = ImGui::InputText("##rename", renameBuf, IM_ARRAYSIZE(renameBuf),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		bool lostFocus = ImGui::IsItemDeactivated();

		if (submitted) {
			CommitRename();
		}
		else if (lostFocus) {
			CancelRename();
		}
	}
	else {
		ImVec2 textPos(textX, rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f);
		dl->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), entry.name.c_str());
	}

	ImGui::PopID();

	if (entry.isDirectory && nodeOpen) {
		for (auto& child : FileManager::getInstance().GetDirectoryContents(entry.virtualPath)) {
			if (filterActive && !SubtreeMatchesFilter(child.virtualPath, filterLower))
				continue;
			DrawNode(child, depth + 1);
		}

		ImGui::TreePop();
	}
}

void FileSystem::ProcessToolbar() {
	ImGui::BeginDisabled(historyIndex <= 0);
	if (ImGui::ArrowButton("##Back", ImGuiDir_Left)) NavigateBack();
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(historyIndex >= (int)history.size() - 1);
	if (ImGui::ArrowButton("##Forward", ImGuiDir_Right)) NavigateForward();
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputText("##SelectedPath", pathDisplayBuf, IM_ARRAYSIZE(pathDisplayBuf), ImGuiInputTextFlags_ReadOnly);
}

void FileSystem::ProcessFilterBar() {
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputTextWithHint("##FilterFiles", "Filter Files", filterBuf, IM_ARRAYSIZE(filterBuf));

	ProcessCreateFolderPopup();
	ProcessCreateScriptPopup();
	ProcessCreateScenePopup();
}

void FileSystem::ProcessCreateFolderPopup() {
	if (newFolderPopupRequested) {
		ImGui::OpenPopup("New Folder");
		newFolderPopupRequested = false;
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputText("##NewFolderName", newFolderNameBuf, IM_ARRAYSIZE(newFolderNameBuf));

		if (ImGui::Button("Create", ImVec2(100, 0))) {
			std::string folderName = newFolderNameBuf;
			if (!folderName.empty()) {
				std::string targetDir = !newFolderTargetPath.empty()
					? newFolderTargetPath
					: FileManager::getInstance().GetRootVirtualPath();

				FileManager::getInstance().CreateFolder(targetDir, folderName);
				expandedPaths.insert(targetDir);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void FileSystem::ProcessCreateScriptPopup() {
	if (newScriptPopupRequested) {
		ImGui::OpenPopup("New Script");
		newScriptPopupRequested = false;
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputTextWithHint("##NewScriptName", "script_name", newScriptNameBuf, IM_ARRAYSIZE(newScriptNameBuf));

		if (ImGui::Button("Create", ImVec2(100, 0))) {
			std::string scriptName = newScriptNameBuf;
			if (!scriptName.empty()) {
				std::string targetDir = !newScriptTargetPath.empty()
					? newScriptTargetPath
					: FileManager::getInstance().GetRootVirtualPath();

				if (FileManager::getInstance().CreateScript(targetDir, scriptName)) {
					expandedPaths.insert(targetDir);
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void FileSystem::ProcessCreateScenePopup() {
	if (newScenePopupRequested) {
		ImGui::OpenPopup("New Scene");
		newScenePopupRequested = false;
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputTextWithHint("##NewSceneName", "scene_name", newSceneNameBuf, IM_ARRAYSIZE(newSceneNameBuf));

		if (ImGui::Button("Create", ImVec2(100, 0))) {
			std::string sceneName = newSceneNameBuf;
			if (!sceneName.empty()) {
				std::string targetDir = !newSceneTargetPath.empty()
					? newSceneTargetPath
					: FileManager::getInstance().GetRootVirtualPath();

				if (FileManager::getInstance().CreateScene(targetDir, sceneName)) {
					expandedPaths.insert(targetDir);
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void FileSystem::ProcessEntryContextMenu(const FileSystemEntry& entry) {
	bool isRoot = entry.virtualPath == FileManager::getInstance().GetRootVirtualPath();

	if (!isRoot) {
		if (ImGui::MenuItem("Rename")) {
			BeginRename(entry.virtualPath, entry.name);
		}
	}

	if (entry.isDirectory) {
		if (ImGui::BeginMenu("Add")) {
			if (ImGui::MenuItem("Folder...")) {
				newFolderTargetPath = entry.virtualPath;
				newFolderNameBuf[0] = '\0';
				newFolderPopupRequested = true;
			}
			if (ImGui::MenuItem("Scene...")) {
				newSceneTargetPath = entry.virtualPath;
				newSceneNameBuf[0] = '\0';
				newScenePopupRequested = true;
			}
			if (ImGui::MenuItem("Script...")) {
				newScriptTargetPath = entry.virtualPath;
				newScriptNameBuf[0] = '\0';
				newScriptPopupRequested = true;
			}
			if (ImGui::MenuItem("File...")) {
				AddFileToFolder(entry.virtualPath);
			}
			ImGui::EndMenu();
		}
	}

	if (!isRoot) {
		if (ImGui::MenuItem("Delete")) {
			FileManager::getInstance().DeleteResource(entry.virtualPath);
			if (selectedPath == entry.virtualPath) {
				selectedPath.clear();
				pathDisplayBuf[0] = '\0';
			}
			if (renamingPath == entry.virtualPath) {
				renamingPath.clear();
			}
		}
	}
}

void FileSystem::ProcessEntries() {
	ImGui::BeginChild("##FileTree", ImVec2(0, 0), true);

	FileSystemEntry root;
	root.name = "res://";
	root.virtualPath = FileManager::getInstance().GetRootVirtualPath();
	root.isDirectory = true;
	root.iconType = ResourceIconType::Folder;

	DrawNode(root, 0);

	ImGui::EndChild();
}

void FileSystem::ProcessWindow() {
	int gen = FileManager::getInstance().GetResourceGeneration();
	if (gen != lastSeenGeneration) {
		lastSeenGeneration = gen;
		selectedPath.clear();
		pathDisplayBuf[0] = '\0';
		history.clear();
		historyIndex = -1;
		expandedPaths.clear();
		expandedPaths.insert(FileManager::getInstance().GetRootVirtualPath());
		renamingPath.clear();
	}

	ImGui::Begin(name.c_str());

	ProcessToolbar();
	ImGui::Dummy(ImVec2(0, 4));
	ProcessFilterBar();
	ImGui::Dummy(ImVec2(0, 4));
	ProcessEntries();

	if (!selectedPath.empty() && renamingPath.empty() &&
		selectedPath != FileManager::getInstance().GetRootVirtualPath() &&
		ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
		std::string currentName = FileManager::getInstance().VirtualToAbsolute(selectedPath).filename().string();
		BeginRename(selectedPath, currentName);
	}

	ImGui::End();
}