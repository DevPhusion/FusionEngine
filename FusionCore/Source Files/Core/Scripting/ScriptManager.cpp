#include "../../../Header Files/Core/Scripting/ScriptManager.h"
#include "../../../Header Files/Core/ObjectManager.h"
#include "../../../Header Files/Components/ScriptComponent.h"
#include "../../../Header Files/Core/Scripting/PyBindings.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
	void SyncFusionRLShimModule(const fs::path& sitePackages) {
		if (sitePackages.empty()) return;

		fs::path shimPath = sitePackages / "fusionRL.py";
		bool wantShim = PackageManager::getInstance().IsPackageInstalled("rl");

		std::error_code ec;
		bool exists = fs::exists(shimPath, ec);

		if (wantShim) {
			if (!exists) {
				std::ofstream out(shimPath);
				if (out.is_open()) out << "from fusion.RL import *\n";
			}
		}
		else if (exists) {
			fs::remove(shimPath, ec);
		}
	}
}

int ScriptManager::RunHiddenCommand(const std::string& command, std::string* outOutput) {
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

ScriptManager::~ScriptManager() {
	if (workerThread.joinable())
		workerThread.join();
	mainThreadGilRelease.reset();
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
	return s == SetupStage::CreatingVenv || s == SetupStage::SyncingPackages ||
		s == SetupStage::GeneratingStubs || s == SetupStage::LinkingInterpreter;
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

	SetStatus(SetupStage::SyncingPackages, "Syncing project packages...");
	PackageManager::getInstance().LoadForProject(projectRoot.string());
	if (PackageManager::getInstance().NeedsSync()) {
		if (!PackageManager::getInstance().SyncInstalledPackages(GetVenvPythonExecutable(venvRoot))) {
			Console::PrintError("ScriptManager: one or more optional packages failed to sync for this project.");
		}
	}

	SyncFusionRLShimModule(GetVenvSitePackages(venvRoot));

	pendingPackageRestart = PackageManager::getInstance().NeedsBindingsRebuild();

	if (!EngineManager::getInstance().isPlayer) {
		SetStatus(SetupStage::GeneratingStubs, "Generating editor stub files...");
		GenerateStubFiles(projectRoot, venvRoot, pendingPackageRestart);
	}

	backgroundSucceeded = true;
	SetStatus(SetupStage::LinkingInterpreter, "Finalizing Python backend...");
}

void ScriptManager::Update() {
	if (stage.load() == SetupStage::LinkingInterpreter && workerThread.joinable()) {
		workerThread.join();

		if (backgroundSucceeded) {
			if (!interpreter) {
				StartEmbeddedInterpreter();
			}
			else if (pendingPackageRestart) {
				RestartEmbeddedInterpreter();
			}
			PackageManager::getInstance().MarkBindingsUpToDate();

			LinkInterpreterToVenv(venvRoot);
			ReloadAllRegisteredScripts();
			SetStatus(SetupStage::Done, "Python backend ready.");
		}
		else {
			SetStatus(SetupStage::Failed, "Python backend setup failed.");
		}
	}
}

void ScriptManager::RestartEmbeddedInterpreter() {
	Console::Print(
		"ScriptManager: project's optional package selection changed the scripting API "
		"surface; restarting the Python backend to rebuild bindings.");

	RunAllScriptsStop();
	ResetDynamicComponentRegistries();

	mainThreadGilRelease.reset();
	interpreter.reset();

	StartEmbeddedInterpreter();
}

void ScriptManager::StartEmbeddedInterpreter() {
	interpreter = std::make_unique<py::scoped_interpreter>();
	mainThreadGilRelease = std::make_unique<py::gil_scoped_release>();
}

bool ScriptManager::CreateVirtualEnvironment(const fs::path& venvPath) {
	std::error_code ec;

	fs::path venvPython = GetVenvPythonExecutable(venvPath);
	if (fs::exists(venvPath / "pyvenv.cfg", ec) && fs::exists(venvPython, ec))
		return true; 

	std::string pyCmd = FindSystemPythonCommand();
	if (pyCmd.empty()) {
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

	fs::path posixStyleExe = venvPath / "bin" / "python.exe";
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

	py::gil_scoped_acquire gil;   

	py::module_ sys = py::module_::import("sys");
	py::list path = sys.attr("path");

	std::string spStr = sitePackages.string();
	for (auto item : path)
		if (item.cast<std::string>() == spStr) return;

	path.attr("insert")(0, spStr);
}

void ScriptManager::GenerateStubFiles(const fs::path& projectDirectory, const fs::path& venvPath,
	bool forceRegenerate) {
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

	fs::path sitePackages = GetVenvSitePackages(venvPath);
	if (sitePackages.empty()) {
		Console::PrintError("ScriptManager: could not determine venv site-packages directory; stub generation will fail.");
		WriteVsCodeSettings(projectDirectory, venvPath, stubDir);
		return;
	}

	fs::path targetCopy = sitePackages / compiledModule.filename();
	bool cachedModuleExisted = fs::exists(targetCopy, ec);
	bool moduleChanged = !cachedModuleExisted || ModulesDiffer(compiledModule, targetCopy);

	bool stubsMissing = !fs::exists(stubDir, ec) || fs::is_empty(stubDir, ec);

	if (!moduleChanged && !stubsMissing && !forceRegenerate) {
		WriteVsCodeSettings(projectDirectory, venvPath, stubDir);
		return;
	}

	if (moduleChanged) {
		fs::copy_file(compiledModule, targetCopy, fs::copy_options::overwrite_existing, ec);
		if (ec) {
			Console::PrintError(
				"ScriptManager: failed to copy compiled module into venv site-packages: {}"
			).Format(ec.message());
		}
	}

	SetStatus(SetupStage::GeneratingStubs, "Installing stub generator...");
	if (!EnsureStubgenInstalled(venvPath)) {
		WriteVsCodeSettings(projectDirectory, venvPath, stubDir);
		return;
	}

	SetStatus(SetupStage::GeneratingStubs, "Generating .pyi stubs...");
	fs::path venvPython = GetVenvPythonExecutable(venvPath);

#if defined(_WIN32)
	_putenv_s("FUSION_PROJECT_DIR", projectDirectory.string().c_str());
#else
	setenv("FUSION_PROJECT_DIR", projectDirectory.string().c_str(), 1);
#endif

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
	else if ((moduleChanged && cachedModuleExisted) || forceRegenerate) {
		Console::Print("ScriptManager: updated python API found, updated automatically");
	}

	fs::path rlStubPath = stubDir / "fusionRL.pyi";
	if (result == 0 && PackageManager::getInstance().IsPackageInstalled("rl")) {
		std::ofstream shim(rlStubPath);
		if (shim.is_open()) {
			shim << "from fusion.RL import *\n";
		}
	}
	else if (fs::exists(rlStubPath, ec)) {
		fs::remove(rlStubPath, ec);
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

bool ScriptManager::ModulesDiffer(const fs::path& lhs, const fs::path& rhs) const {
	std::error_code ec;
	auto lhsSize = fs::file_size(lhs, ec);
	if (ec) return true;
	auto rhsSize = fs::file_size(rhs, ec);
	if (ec) return true;
	if (lhsSize != rhsSize) return true;

	std::ifstream fa(lhs, std::ios::binary);
	std::ifstream fb(rhs, std::ios::binary);
	if (!fa.is_open() || !fb.is_open()) return true;

	constexpr std::size_t kChunkSize = 1 << 16;
	std::vector<char> bufA(kChunkSize), bufB(kChunkSize);
	while (fa && fb) {
		fa.read(bufA.data(), kChunkSize);
		fb.read(bufB.data(), kChunkSize);
		auto readA = fa.gcount();
		auto readB = fb.gcount();
		if (readA != readB) return true;
		if (readA > 0 && std::memcmp(bufA.data(), bufB.data(), static_cast<size_t>(readA)) != 0)
			return true;
	}
	return false;
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

bool ScriptManager::TryRegisterScriptAsComponent(const std::string& virtualScriptPath) {
	if (!Py_IsInitialized()) return false;

	py::gil_scoped_acquire gil;
	try {
		ImportScriptClass(virtualScriptPath);
		return true;
	}
	catch (const py::error_already_set& e) {
		Console::PrintError(
			"ScriptManager: could not auto-register new script '{}': {}"
		).Format(virtualScriptPath, e.what());
	}
	catch (const std::exception& e) {
		Console::PrintError(
			"ScriptManager: could not auto-register new script '{}': {}"
		).Format(virtualScriptPath, e.what());
	}
	return false;
}

void ScriptManager::UnregisterScript(const std::string& scriptVirtualPath) {
	registeredScripts.erase(
		std::remove(registeredScripts.begin(), registeredScripts.end(), scriptVirtualPath),
		registeredScripts.end());
	scriptWriteTimes.erase(scriptVirtualPath);
}

void ScriptManager::RenameRegisteredScript(const std::string& oldVirtualPath, const std::string& newVirtualPath) {
	auto it = std::find(registeredScripts.begin(), registeredScripts.end(), oldVirtualPath);
	if (it != registeredScripts.end())
		*it = newVirtualPath;

	auto wt = scriptWriteTimes.find(oldVirtualPath);
	if (wt != scriptWriteTimes.end()) {
		scriptWriteTimes[newVirtualPath] = wt->second;
		scriptWriteTimes.erase(wt);
	}
}

void ScriptManager::NotifyScriptAttached(const std::string& scriptVirtualPath) {
	if (scriptVirtualPath.empty()) return;

	if (IsScriptLoadedElsewhere(scriptVirtualPath)) {
		return;
	}

	std::filesystem::path absPath = FileManager::getInstance().VirtualToAbsolute(scriptVirtualPath);
	std::error_code ec;
	auto currentWriteTime = std::filesystem::last_write_time(absPath, ec);
	if (ec) return;

	auto it = scriptWriteTimes.find(scriptVirtualPath);
	if (it != scriptWriteTimes.end() && it->second == currentWriteTime) {
		return;
	}

	InvalidateCachedModule(scriptVirtualPath);
	scriptWriteTimes[scriptVirtualPath] = currentWriteTime;
}

bool ScriptManager::IsScriptLoadedElsewhere(const std::string& scriptVirtualPath) const {
	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (script->IsLoaded() && script->GetSourcePath() == scriptVirtualPath) {
					return true;
				}
			}
		}
	}
	return false;
}

void ScriptManager::InvalidateCachedModule(const std::string& scriptVirtualPath) {
	if (!Py_IsInitialized()) return;

	py::gil_scoped_acquire gil;
	std::string moduleName = VirtualPathToModuleName(scriptVirtualPath);

	py::object sysModule = py::module_::import("sys");
	py::dict sysModules = sysModule.attr("modules");
	if (sysModules.contains(moduleName)) {
		sysModules.attr("pop")(moduleName, py::none());
	}
}

void ScriptManager::ReloadAllRegisteredScripts() {
	for (const std::string& scriptPath : registeredScripts) {
		TryRegisterScriptAsComponent(scriptPath); 
	}
}

void ScriptManager::ClearRegisteredScripts() {
	registeredScripts.clear();
	scriptWriteTimes.clear();
}

void ScriptManager::RunAllScriptsLoad() {
	auto& objects = ObjectManager::getInstance().allObjects;
	for (size_t i = 0; i < objects.size(); i++) {
		Object* obj = objects[i].get();
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunOnLoad();
			}
		}
	}
}

void ScriptManager::RunScriptsLoad(const std::vector<Object*>& objects) {
	for (Object* obj : objects) {
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunOnLoad();
			}
		}
	}
}

void ScriptManager::RunAllScriptsStart() {
	auto& objects = ObjectManager::getInstance().allObjects;
	for (size_t i = 0; i < objects.size(); i++) {
		Object* obj = objects[i].get();
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunOnStart();
			}
		}
	}
}

void ScriptManager::RunScriptsStart(const std::vector<Object*>& objects) {
	for (Object* obj : objects) {
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunOnStart();
			}
		}
	}
}

void ScriptManager::RunAllScriptsProcess(float delta) {
	auto& objects = ObjectManager::getInstance().allObjects;
	for (size_t i = 0; i < objects.size(); i++) {
		Object* obj = objects[i].get();
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				if (comp->Enabled) script->RunProcess(delta);
			}
		}
	}
}

void ScriptManager::RunAllScriptsStop() {
	auto& objects = ObjectManager::getInstance().allObjects;
	for (size_t i = 0; i < objects.size(); i++) {
		Object* obj = objects[i].get();
		for (auto& comp : obj->components) {
			if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
				script->Unload();
			}
		}
	}
}