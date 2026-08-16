#include "../../../Header Files/Core/Editor/HeadlessMonitor.h"
#include "../../../Header Files/Core/EngineManager.h"
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
	std::string NormalizeLineEndings(const std::string& s) {
		std::string result;
		result.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n') continue;
			result.push_back(s[i]);
		}
		return result;
	}
}

HeadlessMonitor::~HeadlessMonitor() {
	Stop();
}

bool HeadlessMonitor::Launch(const std::string& fusionFilePath, std::string& outError) {
	if (running.load()) {
		outError = "A headless run is already active.";
		return false;
	}

#ifdef _WIN32
	SECURITY_ATTRIBUTES saAttr{};
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = nullptr;

	if (!CreatePipe(&stdoutReadHandle, &stdoutWriteHandle, &saAttr, 0)) {
		outError = "Failed to create output pipe.";
		return false;
	}
	SetHandleInformation(stdoutReadHandle, HANDLE_FLAG_INHERIT, 0);

	char exePathBuf[MAX_PATH];
	GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
	fs::path exePath(exePathBuf);

	std::ostringstream cmd;
	cmd << "\"" << exePath.string() << "\" --headless \"" << fusionFilePath << "\"";

	STARTUPINFOA si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = stdoutWriteHandle;
	si.hStdError = stdoutWriteHandle;

	processInfo = PROCESS_INFORMATION{};
	std::string cmdStr = cmd.str();
	std::vector<char> buffer(cmdStr.begin(), cmdStr.end());
	buffer.push_back('\0');

	std::string workDir = exePath.parent_path().string();

	BOOL ok = CreateProcessA(
		nullptr, buffer.data(),
		nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW,
		nullptr, workDir.c_str(),
		&si, &processInfo);

	CloseHandle(stdoutWriteHandle); 
	stdoutWriteHandle = nullptr;

	if (!ok) {
		outError = "Failed to launch headless process.";
		CloseHandle(stdoutReadHandle);
		stdoutReadHandle = nullptr;
		return false;
	}

	projectDisplayName = fs::path(fusionFilePath).filename().string();
	stopRequested.store(false);
	running.store(true);

	readerThread = std::thread([this]() { ReaderThreadFunc(); });
	return true;
#else
	outError = "Headless launch is currently only implemented on Windows.";
	return false;
#endif
}

void HeadlessMonitor::ReaderThreadFunc() {
#ifdef _WIN32
	char buf[4096];
	std::string partial;
	DWORD bytesRead = 0;

	while (ReadFile(stdoutReadHandle, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
		partial.append(buf, bytesRead);

		size_t pos;
		while ((pos = partial.find('\x1e')) != std::string::npos) {
			std::string record = partial.substr(0, pos);
			AppendLine(NormalizeLineEndings(record));
			partial.erase(0, pos + 1);

			if (!partial.empty() && partial.front() == '\n') partial.erase(0, 1);
			else if (partial.size() >= 2 && partial[0] == '\r' && partial[1] == '\n') partial.erase(0, 2);
		}
	}
	if (!partial.empty()) AppendLine(NormalizeLineEndings(partial));

	WaitForSingleObject(processInfo.hProcess, INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	CloseHandle(stdoutReadHandle);
	stdoutReadHandle = nullptr;

	AppendLine(stopRequested.load() ? "[Monitor] Headless run stopped." : "[Monitor] Headless run exited.");
	running.store(false);
#endif
}

void HeadlessMonitor::AppendLine(const std::string& line) {
	Console::MessageType type = Console::MessageType::Info;
	std::string text = line;

	auto stripPrefix = [&](const char* prefix, Console::MessageType t) {
		size_t len = std::char_traits<char>::length(prefix);
		if (line.compare(0, len, prefix) == 0) {
			type = t;
			text = line.substr(len);
			return true;
		}
		return false;
		};

	if (!stripPrefix("[Error] ", Console::MessageType::Error) &&
		!stripPrefix("[Warning] ", Console::MessageType::Warning)) {
		stripPrefix("[Info] ", Console::MessageType::Info);
	}

	Console::AddMessage(type, text);
}

void HeadlessMonitor::Stop() {
	if (!running.load()) {
		if (readerThread.joinable()) readerThread.join();
		return;
	}

#ifdef _WIN32
	stopRequested.store(true);
	if (processInfo.hProcess) {
		TerminateProcess(processInfo.hProcess, 0); 
	}
#endif

	if (readerThread.joinable()) readerThread.join();
	running.store(false);
}

void HeadlessMonitor::ProcessMonitorWindow() {
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::Begin("##HeadlessMonitor", nullptr, flags);

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Indent(8.0f);
	ImGui::Text("Headless Run - %s", projectDisplayName.c_str());
	ImGui::Unindent(8.0f);
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Indent(8.0f);

	bool isRunning = running.load();
	ImGui::TextColored(isRunning ? ImVec4(0.35f, 0.85f, 0.4f, 1.0f) : ImVec4(0.85f, 0.35f, 0.35f, 1.0f),
		isRunning ? "Running" : "Stopped");

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::BeginDisabled(!isRunning);
	if (ImGui::Button("Stop"))
		Stop();
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, 12.0f);
	ImGui::Checkbox("Auto-scroll", &autoScroll);

	if (!isRunning) {
		ImGui::SameLine(0.0f, 12.0f);
		if (ImGui::Button("Back to Editor"))
			EngineManager::getInstance().enteringHeadlessMonitor = false;
	}

	ImGui::Unindent(8.0f);
	ImGui::Dummy(ImVec2(0, 8));

	if (ImGui::BeginTabBar("##MonitorTabs")) {
		if (ImGui::BeginTabItem("Console")) {
			headlessConsole.DrawContent();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Agents")) {
			ImGui::TextDisabled("Agent training monitoring will appear here once the RL package reports data.");
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}