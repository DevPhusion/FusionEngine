#include "../../../Header Files/Core/Editor/HeadlessMonitor.h"
#include "../../../Header Files/Core/EngineManager.h"
#include "../../../Header Files/Core/SceneManager.h"
#include "../../../Header Files/Core/Files/FileManager.h"
#include "../../../Header Files/Core/Scripting/ScriptManager.h"
#include <filesystem>
#include <pybind11/embed.h>

namespace py = pybind11;

HeadlessMonitor::~HeadlessMonitor() {
	if (trainingThread.joinable())
		trainingThread.join();
}

void HeadlessMonitor::Start() {
	projectDisplayName = std::filesystem::path(FileManager::getInstance().currentProjectFile).filename().string();

	EngineManager::getInstance().editingScenePath = SceneManager::getInstance().GetCurrentSceneFile();
	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
	EngineManager::getInstance().isHeadless = true;

	Console::Print("[Monitor] Headless run started for " + projectDisplayName + ".");
}

bool HeadlessMonitor::IsRunning() const {
	return EngineManager::getInstance().isHeadless
		&& EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate;
}

void HeadlessMonitor::Stop() {
	if (!IsRunning()) return;

	SceneManager& SM = SceneManager::getInstance();
	const std::string& editingScene = EngineManager::getInstance().editingScenePath;

	if (!editingScene.empty()) {
		SM.LoadSceneFromFile(editingScene);
	}
	else {
		SM.NewScene();
	}

	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Stop);

	Console::Print("[Monitor] Headless run stopped.");
}

void HeadlessMonitor::StartTraining(const TrainConfig& config) {
	if (training.load()) {
		Console::PrintError("[Monitor] Training is already running.");
		return;
	}

	if (!ScriptManager::getInstance().IsReady()) {
		Console::PrintError("[Monitor] Cannot start training: Python backend isn't ready yet "
			"(still setting up the project's virtual environment).");
		return;
	}

	if (trainingThread.joinable())
		trainingThread.join();

	Start(); 
	training.store(true);
	{
		std::lock_guard<std::mutex> lock(trainStatusMutex);
		trainStatus = "Starting...";
		trainError.clear();
	}

	trainingThread = std::thread([this, config]() {
		py::gil_scoped_acquire gil;
		try {
			py::module_ fusionGym = py::module_::import("fusion_gym");
			py::object trainFn = fusionGym.attr("train");

			std::string saveDir = config.saveDir;
			if (saveDir.empty()) {
				std::filesystem::path projectDir =
					std::filesystem::path(FileManager::getInstance().currentProjectFile).parent_path();
				saveDir = (projectDir / "TrainedModels").string();
			}

			trainFn(config.algorithm, config.totalTimesteps, saveDir,
				py::cpp_function([this](std::string msg) {
					std::lock_guard<std::mutex> lock(trainStatusMutex);
					trainStatus = msg;
					}));
		}
		catch (const py::error_already_set& e) {
			std::string msg = e.what();
			Console::PrintError("[Monitor] Training failed: {}").Format(msg);
			std::lock_guard<std::mutex> lock(trainStatusMutex);
			trainError = msg;
		}
		catch (const std::exception& e) {
			std::string msg = e.what();
			Console::PrintError("[Monitor] Training failed: {}").Format(msg);
			std::lock_guard<std::mutex> lock(trainStatusMutex);
			trainError = msg;
		}

		Stop();
		training.store(false);
		});
}

std::string HeadlessMonitor::GetTrainingStatus() const {
	std::lock_guard<std::mutex> lock(trainStatusMutex);
	return trainStatus;
}

std::string HeadlessMonitor::GetTrainingError() const {
	std::lock_guard<std::mutex> lock(trainStatusMutex);
	return trainError;
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

	bool isRunning = IsRunning();
	bool isTraining = IsTraining();

	ImGui::TextColored(isRunning ? ImVec4(0.35f, 0.85f, 0.4f, 1.0f) : ImVec4(0.85f, 0.35f, 0.35f, 1.0f),
		isRunning ? "Running" : "Stopped");

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::Text("%.0f steps/s", EngineManager::getInstance().headlessFps);

	if (isTraining) {
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "%s", GetTrainingStatus().c_str());
	}

	std::string trainErr = GetTrainingError();
	if (!trainErr.empty()) {
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
		ImGui::TextWrapped("Training failed: %s", trainErr.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::BeginDisabled(!isRunning || isTraining);
	if (ImGui::Button("Stop"))
		Stop();
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, 12.0f);
	ImGui::Checkbox("Auto-scroll", &autoScroll);

	if (!isRunning) {
		ImGui::SameLine(0.0f, 12.0f);
		if (ImGui::Button("Back to Editor")) {
			EngineManager::getInstance().isHeadless = false;
		}
	}

	ImGui::Unindent(8.0f);
	ImGui::Dummy(ImVec2(0, 8));

	headlessConsole.DrawContent();

	ImGui::End();
}