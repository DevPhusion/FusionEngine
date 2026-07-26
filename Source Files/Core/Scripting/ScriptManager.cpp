#include "../../../Header Files/Core/Scripting/ScriptManager.h"
#include "../../../Header Files/Core/Scripting/PyBindings.h"
#include "../../../Header Files/Core/ObjectManager.h"
#include "../../../Header Files/Components/ScriptComponent.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace py = pybind11;


PYBIND11_EMBEDDED_MODULE(fusion, m) {
	RegisterEngineBindings(m);
}

namespace {
	int RunHiddenCommand(const std::string& command, std::string* outOutput = nullptr) {
#ifdef _WIN32
		SECURITY_ATTRIBUTES saAttr{};
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = nullptr;

		HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
		if (outOutput) {
			if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0))
				return -1;
			SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
		}

		STARTUPINFOA si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		if (outOutput) {
			si.dwFlags |= STARTF_USESTDHANDLES;
			si.hStdOutput = hWritePipe;
			si.hStdError = hWritePipe;
		}

		PROCESS_INFORMATION pi{};

		std::vector<char> buffer(command.begin(), command.end());
		buffer.push_back('\0');

		BOOL ok = CreateProcessA(
			nullptr, buffer.data(),
			nullptr, nullptr, outOutput ? TRUE : FALSE,
			CREATE_NO_WINDOW,
			nullptr, nullptr,
			&si, &pi);

		if (!ok) {
			if (outOutput) { CloseHandle(hReadPipe); CloseHandle(hWritePipe); }
			return -1;
		}

		if (outOutput) {
			CloseHandle(hWritePipe);

			char readBuf[4096];
			DWORD bytesRead = 0;
			while (ReadFile(hReadPipe, readBuf, sizeof(readBuf), &bytesRead, nullptr) && bytesRead > 0) {
				outOutput->append(readBuf, bytesRead);
			}
			CloseHandle(hReadPipe);
		}

		WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD exitCode = 1;
		GetExitCodeProcess(pi.hProcess, &exitCode);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return static_cast<int>(exitCode);
#else
		if (!outOutput) return std::system(command.c_str());

		std::string cmdWithRedirect = command + " 2>&1";
		FILE* pipe = popen(cmdWithRedirect.c_str(), "r");
		if (!pipe) return -1;

		char buf[4096];
		while (fgets(buf, sizeof(buf), pipe)) *outOutput += buf;

		return pclose(pipe);
#endif
	}
}

ScriptManager::~ScriptManager() {
	if (workerThread.joinable())
		workerThread.join();
	interpreter.reset();
}

void ScriptManager::SetStatus(SetupStage newStage, const std::string& message) {
	{
		std::lock_guard<std::mutex> lock(statusMutex);
		statusMessage = message;
	}
	stage.store(newStage);
}

std::string ScriptManager::GetStatusMessage() const {
	std::lock_guard<std::mutex> lock(statusMutex);
	return statusMessage;
}

bool ScriptManager::IsBusy() const {
	SetupStage s = stage.load();
	return s == SetupStage::CreatingVenv || s == SetupStage::GeneratingStubs || s == SetupStage::LinkingInterpreter;
}

bool ScriptManager::SetupPythonEnvironment(const std::string& projectDirectory) {
	if (projectDirectory.empty()) {
		Console::PrintError("ScriptManager: cannot set up Python environment, project directory is empty.");
		return false;
	}
	if (IsBusy()) return false;

	if (workerThread.joinable())
		workerThread.join();

	projectRoot = fs::path(projectDirectory);
	venvRoot = projectRoot / ".venv";
	backgroundSucceeded = false;

	SetStatus(SetupStage::CreatingVenv, "Creating virtual environment...");

	workerThread = std::thread([this]() { RunBackgroundSetup(); });
	return true;
}

void ScriptManager::RunBackgroundSetup() {
	std::error_code ec;
	fs::create_directories(projectRoot, ec);

	SetStatus(SetupStage::CreatingVenv, "Creating virtual environment (.venv)...");
	if (!CreateVirtualEnvironment(venvRoot)) {
		SetStatus(SetupStage::Failed, "Failed to create Python virtual environment.");
		return;
	}

	SetStatus(SetupStage::GeneratingStubs, "Generating editor stub files...");
	GenerateStubFiles(projectRoot, venvRoot);

	backgroundSucceeded = true;
	SetStatus(SetupStage::LinkingInterpreter, "Finalizing Python backend...");
}

void ScriptManager::Update() {
	if (stage.load() != SetupStage::LinkingInterpreter) return;
	if (!workerThread.joinable()) return;

	workerThread.join();

	if (backgroundSucceeded) {
		if (!interpreter) StartEmbeddedInterpreter();
		LinkInterpreterToVenv(venvRoot);
		SetStatus(SetupStage::Done, "Python backend ready.");
	}
	else {
		SetStatus(SetupStage::Failed, "Python backend setup failed.");
	}
}

void ScriptManager::StartEmbeddedInterpreter() {
	interpreter = std::make_unique<py::scoped_interpreter>();
}

bool ScriptManager::CreateVirtualEnvironment(const fs::path& venvPath) {
	std::error_code ec;

	fs::path venvPython = GetVenvPythonExecutable(venvPath);
	if (fs::exists(venvPath / "pyvenv.cfg", ec) && fs::exists(venvPython, ec))
		return true; // genuinely already set up, reuse it

	std::string pyCmd = FindSystemPythonCommand();
	if (pyCmd.empty()) {
		// FindSystemPythonCommand already logs the specific reason.
		return false;
	}

	fs::create_directories(venvPath.parent_path(), ec);

	std::string command = pyCmd + " -m venv \"" + venvPath.string() + "\"";
	int result = RunHiddenCommand(command);

	return result == 0 && fs::exists(venvPython, ec);
}

std::string ScriptManager::FindSystemPythonCommand() const {
	std::error_code ec;
	if (fs::exists(kBuildPythonPath, ec))
		return std::string("\"") + kBuildPythonPath + "\"";

	Console::PrintError(
		"ScriptManager: expected Python install not found at '{}'. Scripting requires this "
		"exact Python version/build, matching what the engine's stub module was linked against."
	).Format(kBuildPythonPath);

	return "";
}

std::filesystem::path ScriptManager::GetVenvPythonExecutable(const fs::path& venvPath) const {
	std::error_code ec;

	fs::path winStyle = venvPath / "Scripts" / "python.exe";
	if (fs::exists(winStyle, ec)) return winStyle;

	fs::path posixStyleExe = venvPath / "bin" / "python.exe"; // MSYS2/MinGW-style venv layout
	if (fs::exists(posixStyleExe, ec)) return posixStyleExe;

	fs::path posixStyle3 = venvPath / "bin" / "python3";
	if (fs::exists(posixStyle3, ec)) return posixStyle3;

#ifdef _WIN32
	return winStyle;
#else
	return posixStyle3;
#endif
}

std::filesystem::path ScriptManager::GetVenvSitePackages(const fs::path& venvPath) const {
	std::error_code ec;

	fs::path winStyle = venvPath / "Lib" / "site-packages";
	if (fs::exists(winStyle, ec)) return winStyle;

	fs::path posixLibDir = venvPath / "lib";
	if (fs::exists(posixLibDir, ec)) {
		for (auto& entry : fs::directory_iterator(posixLibDir, ec)) {
			if (entry.is_directory() && entry.path().filename().string().rfind("python", 0) == 0) {
				fs::path candidate = entry.path() / "site-packages";
				if (fs::exists(candidate, ec)) return candidate;
			}
		}
	}

	return {};
}

void ScriptManager::LinkInterpreterToVenv(const fs::path& venvPath) {
	fs::path sitePackages = GetVenvSitePackages(venvPath);
	if (sitePackages.empty()) return;

	py::module_ sys = py::module_::import("sys");
	py::list path = sys.attr("path");

	std::string spStr = sitePackages.string();
	for (auto item : path)
		if (item.cast<std::string>() == spStr) return;

	path.attr("insert")(0, spStr);
}

void ScriptManager::GenerateStubFiles(const fs::path& projectDirectory, const fs::path& venvPath) {
	fs::path stubDir = projectDirectory / "typings";
	std::error_code ec;
	fs::create_directories(stubDir, ec);

	fs::path compiledModule = FindCompiledModule();
	if (compiledModule.empty()) {
		Console::PrintError(
			"ScriptManager: could not find a compiled '{}' extension module to generate stubs from; skipping stub generation."
		).Format(kEngineModuleName);
		WriteVsCodeSettings(projectDirectory, venvPath, stubDir);
		return;
	}

	SetStatus(SetupStage::GeneratingStubs, "Installing stub generator...");
	if (!EnsureStubgenInstalled(venvPath)) {
		// EnsureStubgenInstalled already logs the specific reason.
		WriteVsCodeSettings(projectDirectory, venvPath, stubDir);
		return;
	}

	fs::path sitePackages = GetVenvSitePackages(venvPath);
	if (!sitePackages.empty()) {
		fs::path targetCopy = sitePackages / compiledModule.filename();
		if (!fs::exists(targetCopy, ec)) {
			fs::copy_file(compiledModule, targetCopy, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				Console::PrintError(
					"ScriptManager: failed to copy compiled module into venv site-packages: {}"
				).Format(ec.message());
			}
		}
	}
	else {
		Console::PrintError("ScriptManager: could not determine venv site-packages directory; stub generation will fail.");
	}

	SetStatus(SetupStage::GeneratingStubs, "Generating .pyi stubs...");
	fs::path venvPython = GetVenvPythonExecutable(venvPath);
	std::ostringstream cmd;
	cmd << "\"" << venvPython.string() << "\" -m pybind11_stubgen "
		<< kEngineModuleName << " -o \"" << stubDir.string() << "\" --ignore-all-errors";

	std::string stubgenOutput;
	int result = RunHiddenCommand(cmd.str(), &stubgenOutput);
	if (result != 0) {
		Console::PrintError(
			"ScriptManager: pybind11-stubgen exited with code {} : {}. Stubs may be incomplete"
		).Format(result, stubgenOutput);
	}

	WriteVsCodeSettings(projectDirectory, venvPath, stubDir);
}

bool ScriptManager::EnsureStubgenInstalled(const fs::path& venvPath) {
	fs::path venvPython = GetVenvPythonExecutable(venvPath);
	std::error_code ec;
	if (!fs::exists(venvPython, ec)) return false;

	std::string checkOutput;
	std::ostringstream checkCmd;
	checkCmd << "\"" << venvPython.string() << "\" -c \"import pybind11_stubgen\"";
	if (RunHiddenCommand(checkCmd.str(), &checkOutput) == 0)
		return true;

	std::string installOutput;
	std::ostringstream installCmd;
	installCmd << "\"" << venvPython.string() << "\" -m pip install pybind11-stubgen";
	int result = RunHiddenCommand(installCmd.str(), &installOutput);

	if (result != 0) {
		Console::PrintError(
			"ScriptManager: pip install pybind11-stubgen failed (exit code {}): {}"
		).Format(result, installOutput);
	}

	return result == 0;
}

std::filesystem::path ScriptManager::FindCompiledModule() const {
#ifdef _WIN32
	const std::string filename = std::string(kEngineModuleName) + ".pyd";
#else
	const std::string filename = std::string(kEngineModuleName) + ".so";
#endif

	std::error_code ec;

#ifdef _WIN32
	char exePathBuf[MAX_PATH];
	GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
	fs::path exeDir = fs::path(exePathBuf).parent_path();
#else
	fs::path exeDir = fs::canonical("/proc/self/exe", ec).parent_path();
#endif

	fs::path candidate = exeDir / "PythonStubs" / filename;
	if (fs::exists(candidate, ec))
		return candidate;

	candidate = exeDir / filename;
	if (fs::exists(candidate, ec))
		return candidate;

	return {};
}

void ScriptManager::WriteVsCodeSettings(const fs::path& projectDirectory,
	const fs::path& venvPath, const fs::path& stubDir) {

	fs::path vscodeDir = projectDirectory / ".vscode";
	std::error_code ec;
	fs::create_directories(vscodeDir, ec);

	fs::path settingsPath = vscodeDir / "settings.json";
	if (fs::exists(settingsPath, ec)) {
		Console::Print("ScriptManager: {} already exists, using by default").Format(settingsPath.string());
		return;
	}

	std::ofstream out(settingsPath);
	if (!out.is_open()) return;

	out << "{\n"
		<< "\t\"python.defaultInterpreterPath\": \"" << GetVenvPythonExecutable(venvPath).generic_string() << "\",\n"
		<< "\t\"python.analysis.stubPath\": \"" << stubDir.generic_string() << "\",\n"
		<< "\t\"python.analysis.extraPaths\": [\"" << stubDir.generic_string() << "\"]\n"
		<< "}\n";
}

void ScriptManager::RegisterScript(const std::string& scriptVirtualPath) {
	if (std::find(registeredScripts.begin(), registeredScripts.end(), scriptVirtualPath) != registeredScripts.end())
		return;
	registeredScripts.push_back(scriptVirtualPath);
}

void ScriptManager::UnregisterScript(const std::string& scriptVirtualPath) {
	registeredScripts.erase(
		std::remove(registeredScripts.begin(), registeredScripts.end(), scriptVirtualPath),
		registeredScripts.end());
}

void ScriptManager::RenameRegisteredScript(const std::string& oldVirtualPath, const std::string& newVirtualPath) {
	auto it = std::find(registeredScripts.begin(), registeredScripts.end(), oldVirtualPath);
	if (it != registeredScripts.end())
		*it = newVirtualPath;
}

void ScriptManager::ClearRegisteredScripts() {
	registeredScripts.clear();
}

void ScriptManager::RunAllScriptsStart() {
	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunOnStart();
			}
		}
	}
}

void ScriptManager::RunAllScriptsProcess(float delta) {
	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunProcess(delta);
			}
		}
	}
}

void ScriptManager::RunAllScriptsStop() {
	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				script->Unload();
			}
		}
	}
}