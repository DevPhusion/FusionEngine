#include "../../../Header Files/Core/Files/ProjectLauncher.h"

namespace fs = std::filesystem;

namespace {
	std::string FormatFileTime(fs::file_time_type ftime, long long& outEpochSeconds) {
		using namespace std::chrono;

#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
		auto sctp = clock_cast<system_clock>(ftime);
#else
		auto sctp = time_point_cast<system_clock::duration>(
			ftime - fs::file_time_type::clock::now() + system_clock::now());
#endif
		std::time_t tt = system_clock::to_time_t(sctp);
		outEpochSeconds = static_cast<long long>(tt);

		std::tm tmBuf{};
#if defined(_MSC_VER)
		localtime_s(&tmBuf, &tt);
#else
		localtime_r(&tt, &tmBuf);
#endif
		char buf[64];
		std::strftime(buf, sizeof(buf), "%H:%M %d-%m-%Y", &tmBuf);
		return std::string(buf);
	}

	bool FindFusionFileInFolder(const fs::path& folder, fs::path& outFile) {
		std::error_code ec;
		if (!fs::exists(folder, ec) || !fs::is_directory(folder, ec))
			return false;

		for (auto& entry : fs::directory_iterator(folder, ec)) {
			if (ec) break;
			if (entry.is_regular_file() && entry.path().extension() == ".fusion") {
				outFile = entry.path();
				return true;
			}
		}
		return false;
	}

	std::string ToLower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	std::string TrimWhitespace(const std::string& s) {
		size_t start = s.find_first_not_of(" \t");
		size_t end = s.find_last_not_of(" \t");
		if (start == std::string::npos) return "";
		return s.substr(start, end - start + 1);
	}

	std::string SanitizeFileName(const std::string& name) {
		std::string result = name;
		for (char& c : result) {
			if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
				c = '_';
		}
		return result;
	}

	bool IsDirectoryEmpty(const fs::path& folder) {
		std::error_code ec;
		return fs::is_empty(folder, ec) && !ec;
	}
}

void ProjectLauncher::Setup(GLFWwindow* window) {
	this->window = window;
	LoadProjectList();
}

std::string ProjectLauncher::ConfigFilePath() const {
	return "projects.cfg";
}

void ProjectLauncher::LoadProjectList() {
	projects.clear();

	std::ifstream in(ConfigFilePath());
	if (!in.is_open()) return;

	std::string folderLine, nameLine;
	while (std::getline(in, folderLine)) {
		if (!std::getline(in, nameLine)) break; 

		if (!folderLine.empty() && folderLine.back() == '\r') folderLine.pop_back();
		if (!nameLine.empty() && nameLine.back() == '\r') nameLine.pop_back();
		if (folderLine.empty()) continue;

		ProjectEntry entry;
		entry.folderPath = folderLine;
		entry.name = nameLine.empty() ? fs::path(folderLine).filename().string() : nameLine;
		if (entry.name.empty()) entry.name = folderLine;

		RefreshEntry(entry);
		projects.push_back(entry);
	}

	SortProjects();
}

void ProjectLauncher::SaveProjectList() {
	std::ofstream out(ConfigFilePath(), std::ios::trunc);
	if (!out.is_open()) return;

	for (auto& p : projects) {
		out << p.folderPath << "\n";
		out << p.name << "\n";
	}
}

bool ProjectLauncher::RefreshEntry(ProjectEntry& entry) {
	fs::path fusionFile;
	if (!FindFusionFileInFolder(entry.folderPath, fusionFile)) {
		entry.missing = true;
		entry.fusionFilePath.clear();
		entry.lastModifiedText = "Missing";
		return false;
	}

	entry.fusionFilePath = fusionFile.string();
	entry.missing = false;

	std::error_code ec;
	auto writeTime = fs::last_write_time(fusionFile, ec);
	entry.lastModifiedText = ec ? "" : FormatFileTime(writeTime, entry.lastModifiedTime);

	return true;
}

void ProjectLauncher::SortProjects() {
	std::sort(projects.begin(), projects.end(), [](const ProjectEntry& a, const ProjectEntry& b) {
		return a.lastModifiedTime > b.lastModifiedTime;
		});
}

void ProjectLauncher::AddProjectFolder(const std::string& folderPath, const std::string& displayName) {
	for (auto& p : projects) {
		if (p.folderPath == folderPath) {
			if (!displayName.empty()) p.name = displayName;
			RefreshEntry(p);
			SortProjects();
			SaveProjectList();
			return;
		}
	}

	ProjectEntry entry;
	entry.folderPath = folderPath;
	entry.name = !displayName.empty() ? displayName : fs::path(folderPath).filename().string();
	if (entry.name.empty()) entry.name = folderPath;

	RefreshEntry(entry);
	projects.push_back(entry);
	SortProjects();
	SaveProjectList();
}

void ProjectLauncher::RemoveProject(int index) {
	if (index < 0 || index >= (int)projects.size()) return;
	projects.erase(projects.begin() + index);
	selectedIndex = -1;
	SaveProjectList();
}

void ProjectLauncher::ImportProject() {
	auto folder = FileDialog::ShowFolderDialog("Import Project Folder");
	if (!folder) return;

	fs::path fusionFile;
	if (!FindFusionFileInFolder(*folder, fusionFile)) {
		errorMessage = "That folder doesn't contain a .fusion project file.";
		return;
	}

	errorMessage.clear();
	AddProjectFolder(*folder, "");
}

void ProjectLauncher::CreateProjectFromPopup() {
	std::string displayName = TrimWhitespace(newProjectNameBuf);

	if (displayName.empty()) {
		errorMessage = "Please enter a project name.";
		return;
	}
	if (newProjectFolder.empty()) {
		errorMessage = "Please choose a project folder.";
		return;
	}

	std::error_code ec;
	if (!fs::exists(newProjectFolder, ec) || !fs::is_directory(newProjectFolder, ec)) {
		errorMessage = "The selected folder no longer exists.";
		return;
	}
	if (!IsDirectoryEmpty(newProjectFolder)) {
		errorMessage = "Folder must be empty.";
		return;
	}

	std::string safeFileName = SanitizeFileName(displayName);
	fs::path fusionFilePath = fs::path(newProjectFolder) / (safeFileName + ".fusion");

	
	FileManager::getInstance().NewProject();
	FileManager::getInstance().currentProjectFile = fusionFilePath.string();
	FileManager::getInstance().currentProjectDirectory = newProjectFolder;
	FileManager::getInstance().SaveProjectToFile(fusionFilePath.string());
	FileManager::getInstance().SetupResourcesFolder();

	AddProjectFolder(newProjectFolder, displayName);

	PackageManager::getInstance().LoadForProject(newProjectFolder);

	errorMessage.clear();
	pendingEnterProject = true;
}

bool ProjectLauncher::OpenProjectFile(const std::string& fusionFilePath) {
	try {
		FileManager::getInstance().currentProjectFile = fusionFilePath;
		FileManager::getInstance().LoadProjectFromFile(fusionFilePath);

		fs::path folder = fs::path(fusionFilePath).parent_path();
		FileManager::getInstance().currentProjectDirectory = folder.string();
		FileManager::getInstance().SetupResourcesFolder();
		AddProjectFolder(folder.string(), "");

		PackageManager::getInstance().LoadForProject(folder.string());

		pendingEnterProject = true;
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

void ProjectLauncher::ProcessLoadingProjectDisplay(const std::string& message) {
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoSavedSettings;

	ImGui::Begin("##ProjectSetupLoading", nullptr, flags);

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 textSize = ImGui::CalcTextSize(message.c_str());

	ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f));
	ImGui::TextUnformatted(message.c_str());

	ImGui::End();
}

void ProjectLauncher::ProcessNewProjectPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Create New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::Text("Project Name");
		ImGui::SetNextItemWidth(320.0f);
		ImGui::InputText("##NewProjectName", newProjectNameBuf, IM_ARRAYSIZE(newProjectNameBuf));

		ImGui::Dummy(ImVec2(0, 6));
		ImGui::Text("Project Folder");
		ImGui::TextWrapped("%s", newProjectFolder.empty() ? "(no folder selected)" : newProjectFolder.c_str());
		if (ImGui::Button("Browse...")) {
			if (auto folder = FileDialog::ShowFolderDialog("Choose Project Folder"))
				newProjectFolder = *folder;
		}

		bool folderNotEmpty = !newProjectFolder.empty() && !IsDirectoryEmpty(newProjectFolder);
		if (folderNotEmpty) {
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
			ImGui::TextWrapped("Folder must be empty.");
			ImGui::PopStyleColor();
		}
		else if (!errorMessage.empty()) {
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
			ImGui::TextWrapped("%s", errorMessage.c_str());
			ImGui::PopStyleColor();
		}

		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		ImGui::BeginDisabled(folderNotEmpty);
		if (ImGui::Button("Create", ImVec2(120, 0))) {
			CreateProjectFromPopup();
			if (enteredProject)
				ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			errorMessage.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void ProjectLauncher::ProcessConfigurePackagesPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(480, 420), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Configure Packages", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
		PackageManager& pm = PackageManager::getInstance();

		if (selectedIndex >= 0 && selectedIndex < (int)projects.size())
			ImGui::TextDisabled("%s", projects[selectedIndex].name.c_str());
		ImGui::TextWrapped("Selected packages install into this project's own Python "
			"environment the next time it's opened.");
		ImGui::Dummy(ImVec2(0, 6));

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##PackageSearch", "Search packages", packageSearchBuf, IM_ARRAYSIZE(packageSearchBuf));
		std::string search = ToLower(packageSearchBuf);

		ImGui::Dummy(ImVec2(0, 4));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		if (ImGui::BeginTabBar("##PackageTabs")) {

			if (ImGui::BeginTabItem("Selected")) {
				ImGui::Dummy(ImVec2(0, 4));
				bool any = false;

				for (auto& def : pm.GetAvailablePackages()) {
					if (!pm.IsPackageSelected(def.id)) continue;
					if (!search.empty() && ToLower(def.displayName).find(search) == std::string::npos)
						continue;
					any = true;

					ImGui::PushID(def.id.c_str());

					ImGui::TextUnformatted(def.displayName.c_str());
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					ImGui::TextWrapped("%s", def.description.c_str());
					ImGui::PopStyleColor();

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.18f, 0.18f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.22f, 0.22f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.15f, 0.15f, 1.0f));
					if (ImGui::Button("Remove", ImVec2(100, 0)))
						pm.DeselectPackage(def.id);
					ImGui::PopStyleColor(3);

					ImGui::Dummy(ImVec2(0, 10));
					ImGui::PopID();
				}

				if (!any) {
					ImGui::TextDisabled(search.empty()
						? "No packages selected for this project yet."
						: "No selected packages match your search.");
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Available")) {
				ImGui::Dummy(ImVec2(0, 4));
				bool any = false;

				for (auto& def : pm.GetAvailablePackages()) {
					if (pm.IsPackageSelected(def.id)) continue; 
					if (!search.empty() && ToLower(def.displayName).find(search) == std::string::npos)
						continue;
					any = true;

					ImGui::PushID(def.id.c_str());

					ImGui::TextUnformatted(def.displayName.c_str());
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					ImGui::TextWrapped("%s", def.description.c_str());
					ImGui::PopStyleColor();

					if (ImGui::Button("Install", ImVec2(100, 0)))
						pm.SelectPackage(def.id);

					ImGui::Dummy(ImVec2(0, 10));
					ImGui::PopID();
				}

				if (!any) {
					ImGui::TextDisabled(search.empty()
						? "All available packages are already selected."
						: "No available packages match your search.");
				}
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::Dummy(ImVec2(0, 10));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0, 6));

		if (ImGui::Button("Close", ImVec2(120, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

void ProjectLauncher::ProcessLauncher() {
	ScriptManager::getInstance().Update();

	if (pendingEnterProject) {
		if (ScriptManager::getInstance().IsBusy()) {
			ProcessLoadingProjectDisplay("Setting up project: " + ScriptManager::getInstance().GetStatusMessage());
			return; 
		}
		pendingEnterProject = false;
		enteredProject = true;
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::Begin("##ProjectLauncher", nullptr, flags);

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Indent(8.0f);
	ImGui::TextUnformatted("Fusion Engine - Projects");
	ImGui::Unindent(8.0f);
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 6));

	if (ImGui::Button("New Project")) {
		newProjectNameBuf[0] = '\0';
		newProjectFolder.clear();
		errorMessage.clear();
		ImGui::OpenPopup("Create New Project");
	}
	ProcessNewProjectPopup();

	ImGui::SameLine(0.0f, 12.0f);
	if (ImGui::Button("Import"))
		ImportProject();

	ImGui::SameLine(0.0f, 12.0f);
	ImGui::BeginDisabled(selectedIndex < 0);
	if (ImGui::Button("Remove"))
		RemoveProject(selectedIndex);
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, 12.0f);
	ImGui::BeginDisabled(selectedIndex < 0);
	if (ImGui::Button("Configure")) {
		PackageManager::getInstance().LoadForProject(projects[selectedIndex].folderPath);
		packageSearchBuf[0] = '\0';
		ImGui::OpenPopup("Configure Packages");
	}
	ImGui::EndDisabled();
	ProcessConfigurePackagesPopup();

	ImGui::SameLine(0.0f, 24.0f);
	ImGui::SetNextItemWidth(220.0f);
	ImGui::InputTextWithHint("##Filter", "Filter Projects", filterBuf, IM_ARRAYSIZE(filterBuf));

	if (!errorMessage.empty() && !ImGui::IsPopupOpen("Create New Project")) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
		ImGui::TextWrapped("%s", errorMessage.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Dummy(ImVec2(0, 8));

	ImGui::BeginChild("##ProjectListRegion", ImVec2(0, -40.0f), true);

	std::string filter = ToLower(filterBuf);

	for (int i = 0; i < (int)projects.size(); i++) {
		ProjectEntry& p = projects[i];

		if (!filter.empty() && ToLower(p.name).find(filter) == std::string::npos)
			continue;

		ImGui::PushID(i);

		bool selected = (selectedIndex == i);
		ImVec2 rowSize(ImGui::GetContentRegionAvail().x, 48.0f);

		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.28f, 0.40f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.32f, 0.45f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.24f, 0.32f, 0.45f, 0.65f));

		if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick, rowSize)) {
			selectedIndex = i;
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !p.missing)
				OpenProjectFile(p.fusionFilePath);
		}

		ImGui::PopStyleColor(3);

		ImVec2 rectMin = ImGui::GetItemRectMin();

		ImGui::SetCursorScreenPos(ImVec2(rectMin.x + 12.0f, rectMin.y + 6.0f));
		if (p.missing) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
		ImGui::TextUnformatted(p.name.c_str());
		if (p.missing) ImGui::PopStyleColor();

		ImGui::SetCursorScreenPos(ImVec2(rectMin.x + 12.0f, rectMin.y + 26.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextUnformatted(p.folderPath.c_str());
		ImGui::PopStyleColor();

		float dateWidth = ImGui::CalcTextSize(p.lastModifiedText.c_str()).x;
		ImGui::SetCursorScreenPos(ImVec2(rectMin.x + rowSize.x - dateWidth - 12.0f, rectMin.y + 16.0f));
		ImGui::TextUnformatted(p.lastModifiedText.c_str());

		ImGui::PopID();

		ImGui::SetCursorScreenPos(ImVec2(rectMin.x, rectMin.y + rowSize.y));
		ImGui::Dummy(ImVec2(rowSize.x, 6.0f));
	}

	ImGui::EndChild();

	ImGui::Dummy(ImVec2(0, 6));

	bool canOpen = selectedIndex >= 0 && selectedIndex < (int)projects.size() && !projects[selectedIndex].missing;
	ImGui::BeginDisabled(!canOpen);
	if (ImGui::Button("Open Project", ImVec2(140, 0)))
		OpenProjectFile(projects[selectedIndex].fusionFilePath);
	ImGui::EndDisabled();

	ImGui::End();
}