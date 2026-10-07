#include "../../../Header Files/Core/Files/ProjectLauncher.h"
#include "../../../Header Files/Core/Editor/EditorField.h"

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

	void OpenFolderInExplorer(const std::string& folderPath) {
		std::error_code ec;
		if (folderPath.empty() || !fs::is_directory(folderPath, ec)) return;

#ifdef _WIN32
		ShellExecuteW(nullptr, L"open", fs::path(folderPath).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
		std::string cmd = "open \"" + folderPath + "\" >/dev/null 2>&1 &";
		std::system(cmd.c_str());
#else
		std::string cmd = "xdg-open \"" + folderPath + "\" >/dev/null 2>&1 &";
		std::system(cmd.c_str());
#endif
	}

	const ImVec4 kDanger = ImVec4(0.90f, 0.30f, 0.35f, 1.0f);
	const ImVec4 kWarning = ImVec4(0.95f, 0.78f, 0.20f, 1.0f);

	bool AccentButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.27f, 0.29f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorTheme::AccentDim());
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorTheme::Accent());
		bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return pressed;
	}

	void FormLabel(const char* label, float column = 110.0f) {
		ImGui::AlignTextToFramePadding();
		float x = ImGui::GetCursorPosX();
		ImGui::TextDisabled("%s", label);
		ImGui::SameLine(x + column);
	}

	bool PathField(const char* id, const std::string& value, const char* hint) {
		char buf[512];
		std::snprintf(buf, sizeof(buf), "%s", value.c_str());
		const ImGuiStyle& st = ImGui::GetStyle();
		float browseW = ImGui::CalcTextSize("Browse").x + st.FramePadding.x * 2.0f;
		ImGui::SetNextItemWidth(-(browseW + st.ItemSpacing.x));
		ImGui::InputTextWithHint(id, hint, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		std::string btnId = std::string("Browse##") + id;
		return ImGui::Button(btnId.c_str());
	}

	const ImVec4 kBadgePalette[] = {
		ImVec4(0.85f, 0.46f, 0.32f, 1.0f),  // orange
		ImVec4(0.38f, 0.42f, 0.88f, 1.0f),  // indigo
		ImVec4(0.80f, 0.60f, 0.16f, 1.0f),  // amber
		ImVec4(0.70f, 0.32f, 0.72f, 1.0f),  // magenta
		ImVec4(0.28f, 0.54f, 0.84f, 1.0f),  // blue
		ImVec4(0.22f, 0.62f, 0.72f, 1.0f),  // cyan
		ImVec4(0.58f, 0.40f, 0.82f, 1.0f),  // violet
		ImVec4(0.84f, 0.42f, 0.58f, 1.0f),  // rose
	};

	ImVec4 BadgeColorFor(const std::string& key) {
		unsigned int h = 2166136261u;
		for (unsigned char c : key) { h ^= c; h *= 16777619u; }
		return kBadgePalette[h % (sizeof(kBadgePalette) / sizeof(kBadgePalette[0]))];
	}

	std::string BadgeInitials(const std::string& name) {
		std::string stem = fs::path(name).stem().string();
		if (stem.empty()) stem = name;

		std::string out;
		bool startOfWord = true;
		for (unsigned char c : stem) {
			if (c == ' ' || c == '_' || c == '-' || c == '.') { startOfWord = true; continue; }
			if (startOfWord && out.size() < 2) out += (char)std::toupper(c);
			startOfWord = false;
		}
		if (out.size() < 2) {
			out.clear();
			for (unsigned char c : stem) {
				if (std::isalnum(c)) out += (char)std::toupper(c);
				if (out.size() == 2) break;
			}
		}
		return out.empty() ? "?" : out;
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
		entry.hasVersion = false;
		entry.fusionFilePath.clear();
		entry.lastModifiedText = "Missing";
		return false;
	}

	entry.fusionFilePath = fusionFile.string();
	entry.missing = false;
	entry.hasVersion = FileManager::getInstance().ReadProjectVersion(entry.fusionFilePath, entry.version);

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

void ProjectLauncher::RequestOpenProject(const std::string& fusionFilePath, const std::string& name,
	bool hasVersion, const std::string& version) {
	if (!hasVersion || version != FileManager::version) {
		pendingOpenPath = fusionFilePath;
		versionWarningName = name;
		versionWarningValue = version;
		versionWarningHasValue = hasVersion;
		versionWarningRequested = true;
		return;
	}
	OpenProjectFile(fusionFilePath);
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
	std::error_code ec;
	if (fusionFilePath.empty() || fs::path(fusionFilePath).parent_path().empty() ||
		!fs::exists(fusionFilePath, ec) || ec) {
		errorMessage = "Could not open project: '" + fusionFilePath + "' is not a valid project file.";
		return false;
	}

	try {
		FileManager::getInstance().LoadProjectFromFile(fusionFilePath);   

		fs::path folder = fs::path(fusionFilePath).parent_path();
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
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

	ImGui::Begin("##ProjectSetupLoading", nullptr, flags);

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 center(origin.x + avail.x * 0.5f, origin.y + avail.y * 0.5f - 20.0f);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float radius = 16.0f;
	const float t = (float)ImGui::GetTime();
	const ImVec4 a = ImVec4(0.75f, 0.76f, 0.78f, 1.0f);
	for (int i = 0; i < 10; i++) {
		float angle = t * 6.0f + (float)i * (2.0f * 3.14159265f / 10.0f);
		float alpha = 0.15f + 0.85f * (float)i / 10.0f;
		ImVec2 p(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius);
		dl->AddCircleFilled(p, 3.0f, ImGui::GetColorU32(ImVec4(a.x, a.y, a.z, alpha)));
	}

	ImVec2 ts = ImGui::CalcTextSize("Setting up project");
	ImGui::SetCursorScreenPos(ImVec2(center.x - ts.x * 0.5f, center.y + radius + 20.0f));
	EditorField::BoldText("Setting up project");

	ImVec2 ms = ImGui::CalcTextSize(message.c_str());
	ImGui::SetCursorScreenPos(ImVec2(center.x - ms.x * 0.5f, center.y + radius + 44.0f));
	ImGui::TextDisabled("%s", message.c_str());

	ImGui::End();
}

void ProjectLauncher::ProcessVersionWarningPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Always);

	if (!ImGui::BeginPopupModal("Version Mismatch", nullptr,
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) return;

	const std::string& engineV = FileManager::version;
	const std::string projV = versionWarningHasValue ? versionWarningValue : "unknown";

	ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
	ImGui::TextWrapped("The version of \"%s\" (%s) does not match this engine (%s).",
		versionWarningName.c_str(), projV.c_str(), engineV.c_str());
	ImGui::PopStyleColor();

	ImGui::Spacing();
	ImGui::TextWrapped("Opening it may cause data corruption or crashes. Saving it afterwards will overwrite the file.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	const ImGuiStyle& st = ImGui::GetStyle();
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 130.0f * 2.0f - st.ItemSpacing.x - st.WindowPadding.x);

	bool openAnyway = ImGui::Button("Open Anyway", ImVec2(130, 0));
	ImGui::SameLine();
	bool cancel = ImGui::Button("Cancel", ImVec2(130, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

	if (openAnyway) {
		std::string path = std::move(pendingOpenPath);
		pendingOpenPath.clear();
		ImGui::CloseCurrentPopup();
		OpenProjectFile(path);
	}
	else if (cancel) {
		pendingOpenPath.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void ProjectLauncher::ProcessNewProjectPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);

	if (!ImGui::BeginPopupModal("Create New Project", nullptr,
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) return;

	if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

	FormLabel("Name");
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##NewProjectName", "My Project", newProjectNameBuf, IM_ARRAYSIZE(newProjectNameBuf));

	FormLabel("Folder");
	if (PathField("##NewProjectFolder", newProjectFolder, "Choose an empty folder...")) {
		if (auto folder = FileDialog::ShowFolderDialog("Choose Project Folder"))
			newProjectFolder = *folder;
	}

	const bool folderNotEmpty = !newProjectFolder.empty() && !IsDirectoryEmpty(newProjectFolder);
	if (folderNotEmpty) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
		ImGui::TextWrapped("This folder isn't empty. Choose an empty folder.");
		ImGui::PopStyleColor();
	}
	else if (!errorMessage.empty()) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
		ImGui::TextWrapped("%s", errorMessage.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	const ImGuiStyle& st = ImGui::GetStyle();
	const float btnW = 110.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnW * 2.0f - st.ItemSpacing.x - st.WindowPadding.x);

	ImGui::BeginDisabled(folderNotEmpty);
	bool create = AccentButton("Create", ImVec2(btnW, 0));
	ImGui::EndDisabled();
	if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !folderNotEmpty) create = true;

	ImGui::SameLine();
	bool cancel = ImGui::Button("Cancel", ImVec2(btnW, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

	if (create) {
		CreateProjectFromPopup();
		if (errorMessage.empty()) ImGui::CloseCurrentPopup();
	}
	if (cancel) {
		errorMessage.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void ProjectLauncher::ProcessConfigurePackagesPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_Appearing);

	if (!ImGui::BeginPopupModal("Configure Packages", nullptr, ImGuiWindowFlags_NoSavedSettings)) return;

	PackageManager& pm = PackageManager::getInstance();
	const ImGuiStyle& st = ImGui::GetStyle();

	if (selectedIndex >= 0 && selectedIndex < (int)projects.size())
		EditorField::BoldText(projects[selectedIndex].name.c_str());
	ImGui::TextDisabled("Packages install into this project's Python environment the next time it's opened.");
	ImGui::Spacing();

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##PackageSearch", "Search packages...", packageSearchBuf, IM_ARRAYSIZE(packageSearchBuf));
	const std::string search = ToLower(packageSearchBuf);
	ImGui::Spacing();

	const float footerH = ImGui::GetFrameHeight() + st.ItemSpacing.y * 2.0f + 6.0f;

	auto drawList = [&](bool selectedTab) {
		ImGui::BeginChild(selectedTab ? "##SelList" : "##AvailList", ImVec2(0, -footerH), ImGuiChildFlags_Borders);
		bool any = false;

		for (auto& def : pm.GetAvailablePackages()) {
			if (pm.IsPackageSelected(def.id) != selectedTab) continue;
			if (!search.empty() && ToLower(def.displayName).find(search) == std::string::npos) continue;
			any = true;

			ImGui::PushID(def.id.c_str());

			const float btnW = 90.0f;
			ImGui::BeginGroup();
			EditorField::BoldText(def.displayName.c_str());
			ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - btnW - st.ItemSpacing.x * 2.0f + ImGui::GetCursorPosX());
			ImGui::TextDisabled("%s", def.description.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();

			float blockH = ImGui::GetItemRectSize().y;
			float rowTop = ImGui::GetItemRectMin().y;
			ImGui::SameLine(ImGui::GetWindowWidth() - btnW - st.WindowPadding.x);
			ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x,
				rowTop + (blockH - ImGui::GetFrameHeight()) * 0.5f));

			if (selectedTab) {
				if (EditorField::DangerButton("Remove", ImVec2(btnW, 0))) pm.DeselectPackage(def.id);
			}
			else {
				if (AccentButton("Install", ImVec2(btnW, 0))) pm.SelectPackage(def.id);
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::PopID();
		}

		if (!any) {
			ImGui::TextDisabled("%s", !search.empty() ? "No packages match your search."
				: selectedTab ? "No packages selected for this project yet."
				: "All available packages are already selected.");
		}
		ImGui::EndChild();
		};

	if (ImGui::BeginTabBar("##PackageTabs")) {
		if (ImGui::BeginTabItem("Selected")) { drawList(true); ImGui::EndTabItem(); }
		if (ImGui::BeginTabItem("Available")) { drawList(false); ImGui::EndTabItem(); }
		ImGui::EndTabBar();
	}

	ImGui::Separator();
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110.0f - st.WindowPadding.x);
	if (ImGui::Button("Close", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
}

void ProjectLauncher::ProcessLauncher() {
	ScriptManager::getInstance().Update();

	if (pendingEnterProject) {
		if (ScriptManager::getInstance().IsBusy()) {
			ProcessLoadingProjectDisplay(ScriptManager::getInstance().GetStatusMessage());
			return;
		}
		pendingEnterProject = false;
		enteredProject = true;
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 22.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##ProjectLauncher", nullptr, flags);
	ImGui::PopStyleVar(2);

	const ImGuiStyle& st = ImGui::GetStyle();

	{
		bool b = EditorTheme::PushBold();
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextUnformatted("Fusion Engine");
		ImGui::SetWindowFontScale(1.0f);
		EditorTheme::PopBold(b);
		ImGui::TextDisabled("Create, import and open your projects");
	}
	ImGui::Spacing();
	ImGui::Spacing();

	{
		const float newW = ImGui::CalcTextSize("+  New Project").x + st.FramePadding.x * 2.0f;
		const float importW = ImGui::CalcTextSize("Import").x + st.FramePadding.x * 2.0f;
		const float rightW = importW + newW + st.ItemSpacing.x;

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rightW - st.ItemSpacing.x);
		ImGui::InputTextWithHint("##Filter", "Search projects...", filterBuf, IM_ARRAYSIZE(filterBuf));

		ImGui::SameLine();
		if (ImGui::Button("Import")) ImportProject();

		ImGui::SameLine();
		if (AccentButton("+  New Project")) {
			newProjectNameBuf[0] = '\0';
			newProjectFolder.clear();
			errorMessage.clear();
			ImGui::OpenPopup("Create New Project");
		}
		ProcessNewProjectPopup();
	}

	if (!errorMessage.empty() && !ImGui::IsPopupOpen("Create New Project")) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
		ImGui::TextWrapped("%s", errorMessage.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();

	const float footerH = ImGui::GetFrameHeight() + st.ItemSpacing.y + 14.0f
		+ ImGui::GetTextLineHeight() + st.ItemSpacing.y;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::BeginChild("##ProjectListRegion", ImVec2(0, -footerH));
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	const std::string filter = ToLower(filterBuf);
	int shown = 0;
	int openIndex = -1, removeIndex = -1, configureIndex = -1;

	for (int i = 0; i < (int)projects.size(); i++) {
		ProjectEntry& p = projects[i];
		if (!filter.empty() && ToLower(p.name).find(filter) == std::string::npos) continue;
		shown++;

		ImGui::PushID(i);

		const float cardH = 64.0f;
		const float rounding = 8.0f;
		const float cardW = ImGui::GetContentRegionAvail().x;
		const bool selected = (selectedIndex == i);

		const ImVec2 mn = ImGui::GetCursorScreenPos();
		const ImVec2 mx(mn.x + cardW, mn.y + cardH);

		ImGui::InvisibleButton("##card", ImVec2(cardW, cardH));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) selectedIndex = i;
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !p.missing) openIndex = i;

		if (ImGui::BeginPopupContextItem("##ctx")) {
			if (ImGui::MenuItem("Open", nullptr, false, !p.missing)) openIndex = i;
			if (ImGui::MenuItem("Configure Packages...")) configureIndex = i;
			if (ImGui::MenuItem("Open Folder in Explorer")) {
				OpenFolderInExplorer(p.folderPath);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Remove from list")) removeIndex = i;
			ImGui::EndPopup();
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		
		const ImVec4 accent = EditorTheme::Accent();

		const ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
		const ImVec4 selTint = ImVec4(
			base.x + (accent.x - base.x) * 0.10f,
			base.y + (accent.y - base.y) * 0.10f,
			base.z + (accent.z - base.z) * 0.10f,
			1.0f);

		ImU32 bg = ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
		if (selected) bg = ImGui::GetColorU32(selTint);
		dl->AddRectFilled(mn, mx, bg, rounding);
		dl->AddRect(mn, mx,
			selected ? ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.70f)) : ImGui::GetColorU32(ImGuiCol_Border),
			rounding, 0, 1.0f);

		const float badge = 40.0f;
		const ImVec2 bmn(mn.x + 14.0f, mn.y + (cardH - badge) * 0.5f);
		const ImVec2 bmx(bmn.x + badge, bmn.y + badge);
		const ImVec4 badgeCol = p.missing ? ImVec4(0.45f, 0.18f, 0.20f, 1.0f) : BadgeColorFor(p.folderPath);
		dl->AddRectFilled(bmn, bmx, ImGui::GetColorU32(badgeCol), 7.0f);
		if (selected)
			dl->AddRect(bmn, bmx, IM_COL32(255, 255, 255, 110), 7.0f, 0, 1.5f);   // light ring so it pops

		{
			const std::string initials = BadgeInitials(p.name);
			bool b = EditorTheme::PushBold();
			ImVec2 ls = ImGui::CalcTextSize(initials.c_str());
			ImGui::SetCursorScreenPos(ImVec2(bmn.x + (badge - ls.x) * 0.5f, bmn.y + (badge - ls.y) * 0.5f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
			ImGui::TextUnformatted(initials.c_str());
			ImGui::PopStyleColor();
			EditorTheme::PopBold(b);
		}

		const char* rightText = p.missing ? "Missing" : p.lastModifiedText.c_str();
		const float dateW = ImGui::CalcTextSize(rightText).x;
		const float textY = mn.y + (cardH - ImGui::GetTextLineHeight()) * 0.5f;
		const float dateX = mx.x - dateW - 18.0f;

		ImGui::SetCursorScreenPos(ImVec2(dateX, textY));
		ImGui::PushStyleColor(ImGuiCol_Text, p.missing ? kDanger : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextUnformatted(rightText);
		ImGui::PopStyleColor();

		float rightmostLeft = dateX;   

		if (!p.missing) {
			const bool mismatch = !p.hasVersion || p.version != FileManager::version;
			const std::string vText = p.hasVersion ? p.version : "?";
			const ImVec2 vSize = ImGui::CalcTextSize(vText.c_str());
			const ImVec2 vPos(dateX - 16.0f - vSize.x, textY);

			ImGui::SetCursorScreenPos(vPos);
			ImGui::PushStyleColor(ImGuiCol_Text, mismatch ? kWarning : ImGui::GetStyleColorVec4(ImGuiCol_Text));
			ImGui::TextUnformatted(vText.c_str());
			ImGui::PopStyleColor();

			if (mismatch && ImGui::IsWindowHovered() &&
				ImGui::IsMouseHoveringRect(vPos, ImVec2(vPos.x + vSize.x, vPos.y + vSize.y))) {
				ImGui::SetTooltip("Version doesn't match the engine (%s).", FileManager::version.c_str());
			}
			rightmostLeft = vPos.x;
		}

		const float textX = bmx.x + 14.0f;
		ImGui::PushClipRect(ImVec2(textX, mn.y), ImVec2(rightmostLeft - 14.0f, mx.y), true);

		ImGui::SetCursorScreenPos(ImVec2(textX, mn.y + 11.0f));
		EditorField::BoldText(p.name.c_str(), p.missing ? &kDanger : nullptr);

		ImGui::SetCursorScreenPos(ImVec2(textX, mn.y + 34.0f));
		ImGui::TextDisabled("%s", p.folderPath.c_str());

		ImGui::PopClipRect();

		ImGui::SetCursorScreenPos(ImVec2(mn.x, mx.y));
		ImGui::Dummy(ImVec2(cardW, 8.0f));

		ImGui::PopID();
	}

	if (shown == 0) {
		ImGui::Dummy(ImVec2(0, 50.0f));
		const char* title = projects.empty() ? "No projects yet" : "No projects match your search";
		const char* sub = projects.empty() ? "Create a new project or import an existing one to get started." : "Try a different search term.";
		const float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((w - ImGui::CalcTextSize(title).x) * 0.5f);
		EditorField::BoldText(title);
		ImGui::SetCursorPosX((w - ImGui::CalcTextSize(sub).x) * 0.5f);
		ImGui::TextDisabled("%s", sub);
	}

	ImGui::EndChild();

	ImGui::Separator();
	ImGui::Spacing();

	const bool hasSel = selectedIndex >= 0 && selectedIndex < (int)projects.size();
	const bool canOpen = hasSel && !projects[selectedIndex].missing;

	ImGui::BeginDisabled(!hasSel);
	if (ImGui::Button("Configure Packages")) configureIndex = selectedIndex;
	ImGui::SameLine();
	if (EditorField::DangerButton("Remove")) removeIndex = selectedIndex;
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%d project%s", (int)projects.size(), projects.size() == 1 ? "" : "s");

	const float openW = 150.0f;
	ImGui::SameLine(ImGui::GetWindowWidth() - openW - st.WindowPadding.x);
	ImGui::BeginDisabled(!canOpen);
	if (AccentButton("Open Project", ImVec2(openW, 0))) openIndex = selectedIndex;
	ImGui::EndDisabled();

	const std::string engineText = "Official Release " + FileManager::version;
	const float w = ImGui::CalcTextSize(engineText.c_str()).x;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - st.WindowPadding.x);
	ImGui::TextDisabled("%s", engineText.c_str());

	if (configureIndex >= 0 && configureIndex < (int)projects.size()) {
		selectedIndex = configureIndex;
		PackageManager::getInstance().LoadForProject(projects[configureIndex].folderPath);
		packageSearchBuf[0] = '\0';
		ImGui::OpenPopup("Configure Packages");
	}
	ProcessConfigurePackagesPopup();

	if (versionWarningRequested) {
		ImGui::OpenPopup("Version Mismatch");
		versionWarningRequested = false;
	}
	ProcessVersionWarningPopup();

	if (removeIndex >= 0) RemoveProject(removeIndex);
	else if (openIndex >= 0 && openIndex < (int)projects.size() && !projects[openIndex].missing) {
		const ProjectEntry& p = projects[openIndex];
		RequestOpenProject(p.fusionFilePath, p.name, p.hasVersion, p.version);
	}

	ImGui::End();
}