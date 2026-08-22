#pragma once

#include <pybind11/embed.h>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "../Editor/Windows/Console.h"
#include "PackageManager.h"
#include "../../Objects/Object.h"

namespace fs = std::filesystem;
namespace py = pybind11;

enum class SetupStage {
	Idle,
	CreatingVenv,
	SyncingPackages,
	GeneratingStubs,
	LinkingInterpreter,
	Done,
	Failed
};

class ScriptManager
{
public:
	static ScriptManager& getInstance() {
		static ScriptManager instance;
		return instance;
	}

	ScriptManager(const ScriptManager&) = delete;
	void operator=(const ScriptManager&) = delete;

	static constexpr const char* kEngineModuleName = "fusion";

	static constexpr const char* kBuildPythonPath =
		"C:\\Users\\User\\AppData\\Local\\Programs\\Python\\Python311\\python.exe";

	int RunHiddenCommand(const std::string& command, std::string* outOutput = nullptr);
	
	void RunAllScriptsLoad();
	void RunAllScriptsStart();
	void RunScriptsLoad(const std::vector<Object*>& objects);
	void RunScriptsStart(const std::vector<Object*>& objects);
	void RunAllScriptsProcess(float delta);
	void RunAllScriptsStop();

	std::vector<std::string> registeredScripts;
	bool TryRegisterScriptAsComponent(const std::string& virtualScriptPath);
	void RegisterScript(const std::string& scriptVirtualPath);
	void UnregisterScript(const std::string& scriptVirtualPath);
	void RenameRegisteredScript(const std::string& oldVirtualPath, const std::string& newVirtualPath);
	void ClearRegisteredScripts();

	void NotifyScriptAttached(const std::string& scriptVirtualPath);

	bool SetupPythonEnvironment(const std::string& projectDirectory);

	void Update();

	SetupStage GetStage() const { return stage.load(); }
	bool IsBusy() const;
	bool IsReady() const { return stage.load() == SetupStage::Done; }
	std::string GetStatusMessage() const;

	bool IsInitialized() const { return interpreter != nullptr; }
	const std::filesystem::path& GetVenvRoot() const { return venvRoot; }

private:
	ScriptManager() = default;
	~ScriptManager();

	std::unordered_map<std::string, std::filesystem::file_time_type> scriptWriteTimes;
	std::chrono::steady_clock::time_point nextGlobalFileCheckTime{};
	static constexpr std::chrono::milliseconds kGlobalFileWatchInterval{ 500 };

	bool IsScriptLoadedElsewhere(const std::string& scriptVirtualPath) const;
	void InvalidateCachedModule(const std::string& scriptVirtualPath);
	void ReloadAllRegisteredScripts();

	void RunBackgroundSetup();
	void SetStatus(SetupStage newStage, const std::string& message);

	bool CreateVirtualEnvironment(const std::filesystem::path& venvPath);
	void StartEmbeddedInterpreter();
	void LinkInterpreterToVenv(const std::filesystem::path& venvPath);

	void GenerateStubFiles(const std::filesystem::path& projectDirectory, const std::filesystem::path& venvPath,
		bool forceRegenerate);
	bool EnsureStubgenInstalled(const std::filesystem::path& venvPath);
	std::filesystem::path FindCompiledModule() const;
	bool ModulesDiffer(const fs::path& lhs, const fs::path& rhs) const;

	void WriteVsCodeSettings(const std::filesystem::path& projectDirectory,
		const std::filesystem::path& venvPath, const std::filesystem::path& stubDir);

	std::string FindSystemPythonCommand() const;
	std::filesystem::path GetVenvPythonExecutable(const std::filesystem::path& venvPath) const;
	std::filesystem::path GetVenvSitePackages(const std::filesystem::path& venvPath) const;

	bool pendingPackageRestart = false;
	void RestartEmbeddedInterpreter();

	std::unique_ptr<pybind11::scoped_interpreter> interpreter;
	std::unique_ptr<pybind11::gil_scoped_release> mainThreadGilRelease;
	std::filesystem::path venvRoot;
	std::filesystem::path projectRoot;

	std::thread workerThread;
	std::atomic<SetupStage> stage{ SetupStage::Idle };
	std::atomic<bool> backgroundSucceeded{ false };

	mutable std::mutex statusMutex;
	std::string statusMessage;
};