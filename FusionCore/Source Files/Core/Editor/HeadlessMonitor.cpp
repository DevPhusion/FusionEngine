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

void HeadlessMonitor::SerializeTrainConfig(BinaryWriter& w) {
	w.Write(config.totalTimesteps);
	w.WriteString(config.algorithm);
	w.WriteString(config.saveDir);
}

void HeadlessMonitor::DeserializeTrainConfig(BinaryReader& r) {
	config.totalTimesteps = r.Read<long long>();
	config.algorithm = r.ReadString();
	config.saveDir = r.ReadString();
}

void HeadlessMonitor::Begin() {
	projectDisplayName = std::filesystem::path(FileManager::getInstance().currentProjectFile).filename().string();

	EngineManager::getInstance().editingScenePath = SceneManager::getInstance().GetCurrentSceneFile();
	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
	EngineManager::getInstance().isHeadless = true;

	Console::Print("[Training] Training started for " + projectDisplayName + ".");
}

void HeadlessMonitor::End() {
	SceneManager& SM = SceneManager::getInstance();
	const std::string& editingScene = EngineManager::getInstance().editingScenePath;

	if (!editingScene.empty()) {
		SM.LoadSceneFromFile(editingScene);
	}
	else {
		SM.NewScene();
	}

	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Stop);

	Console::Print("[Training] Training finished.");
}

void HeadlessMonitor::StartTraining(const TrainConfig& config) {
	if (training.load()) {
		Console::PrintError("[Training] Training is already running.");
		return;
	}

	if (!ScriptManager::getInstance().IsReady()) {
		Console::PrintError("[Training] Cannot start training: Python backend isn't ready yet "
			"(still setting up the project's virtual environment).");
		return;
	}

	if (trainingThread.joinable())
		trainingThread.join();

	this->config = config;

	Begin();
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
			Console::PrintError("[Training] Training failed: {}").Format(msg);
			std::lock_guard<std::mutex> lock(trainStatusMutex);
			trainError = msg;
		}
		catch (const std::exception& e) {
			std::string msg = e.what();
			Console::PrintError("[Training] Training failed: {}").Format(msg);
			std::lock_guard<std::mutex> lock(trainStatusMutex);
			trainError = msg;
		}

		pendingEnd.store(true);
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
	EngineManager::getInstance().ProcessPendingMainThreadTasks();

	if (pendingEnd.exchange(false)) {
		End();
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::Begin("##HeadlessMonitor", nullptr, flags);

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Indent(8.0f);
	ImGui::Text("Training - %s", projectDisplayName.c_str());
	ImGui::Unindent(8.0f);
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Indent(8.0f);

	bool isTraining = IsTraining();

	ImGui::TextColored(isTraining ? ImVec4(0.35f, 0.85f, 0.4f, 1.0f) : ImVec4(0.85f, 0.35f, 0.35f, 1.0f),
		isTraining ? "Training" : "Finished");

	if (isTraining) {
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "%s", GetTrainingStatus().c_str());
	}

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::Checkbox("Auto-scroll", &autoScroll);

	if (!isTraining) {
		ImGui::SameLine(0.0f, 12.0f);
		if (ImGui::Button("Back to Editor")) {
			EngineManager::getInstance().isHeadless = false;
		}
	}

	std::string trainErr = GetTrainingError();
	if (!trainErr.empty()) {
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
		ImGui::TextWrapped("Training failed: %s", trainErr.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Unindent(8.0f);
	ImGui::Dummy(ImVec2(0, 8));

	headlessConsole.DrawContent();

	ImGui::End();
}