#pragma once

#include <pybind11/embed.h>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "../../../Header Files/Core/Editor/Windows/Console.h"

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

	enum class SetupStage {
		Idle,
		CreatingVenv,
		GeneratingStubs,
		LinkingInterpreter, 
		Done,
		Failed
	};

	bool SetupPythonEnvironment(const std::string& projectDirectory);

	void Update();

	SetupStage GetStage() const { return stage.load(); }
	bool IsBusy() const;
	std::string GetStatusMessage() const;

	bool IsInitialized() const { return interpreter != nullptr; }
	const std::filesystem::path& GetVenvRoot() const { return venvRoot; }

private:
	ScriptManager() = default;
	~ScriptManager();

	void RunBackgroundSetup();
	void SetStatus(SetupStage newStage, const std::string& message);

	bool CreateVirtualEnvironment(const std::filesystem::path& venvPath);
	void StartEmbeddedInterpreter();
	void LinkInterpreterToVenv(const std::filesystem::path& venvPath);

	void GenerateStubFiles(const std::filesystem::path& projectDirectory, const std::filesystem::path& venvPath);
	bool EnsureStubgenInstalled(const std::filesystem::path& venvPath);
	std::filesystem::path FindCompiledModule() const;

	void WriteVsCodeSettings(const std::filesystem::path& projectDirectory,
		const std::filesystem::path& venvPath, const std::filesystem::path& stubDir);

	std::string FindSystemPythonCommand() const;
	std::filesystem::path GetVenvPythonExecutable(const std::filesystem::path& venvPath) const;
	std::filesystem::path GetVenvSitePackages(const std::filesystem::path& venvPath) const;

	std::unique_ptr<pybind11::scoped_interpreter> interpreter;
	std::filesystem::path venvRoot;
	std::filesystem::path projectRoot;

	std::thread workerThread;
	std::atomic<SetupStage> stage{ SetupStage::Idle };
	std::atomic<bool> backgroundSucceeded{ false };

	mutable std::mutex statusMutex;
	std::string statusMessage;
};