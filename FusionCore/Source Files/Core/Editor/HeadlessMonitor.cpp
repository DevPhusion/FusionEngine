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
	w.WriteString(config.startFromModelPath);
}

void HeadlessMonitor::DeserializeTrainConfig(BinaryReader& r) {
	config.totalTimesteps = r.Read<long long>();
	config.algorithm = r.ReadString();
	config.saveDir = r.ReadString();
	config.startFromModelPath = r.ReadString();
}

void HeadlessMonitor::Begin() {
	projectDisplayName = std::filesystem::path(FileManager::getInstance().currentProjectFile).filename().string();

	EngineManager::getInstance().editingScenePath = SceneManager::getInstance().GetCurrentSceneFile();
	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
	EngineManager::getInstance().isHeadless = true;

	{
		std::lock_guard<std::mutex> lock(metricsMutex);
		metricSeries.clear();
		metricStepCounter = 0;
	}

	Console::Print("[Training] Training started for " + projectDisplayName + ".");
}

void HeadlessMonitor::End() {
	EngineManager::getInstance().isHeadless = false;

	SceneManager& SM = SceneManager::getInstance();
	const std::string& editingScene = EngineManager::getInstance().editingScenePath;

	if (!editingScene.empty()) {
		SM.LoadSceneFromFile(editingScene);
	}
	else {
		SM.NewScene();
	}

	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Stop);
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

			std::string startFromAbsPath;
			if (!config.startFromModelPath.empty()) {
				startFromAbsPath = FileManager::getInstance()
					.VirtualToAbsolute(config.startFromModelPath).string();
			}

			trainFn(config.algorithm, config.totalTimesteps, saveDir, startFromAbsPath,
				py::cpp_function([this](std::string msg) {
					std::lock_guard<std::mutex> lock(trainStatusMutex);
					trainStatus = msg;
					}),
				py::cpp_function([this](py::dict data) {
					double x = -1.0;
					if (data.contains("time/total_timesteps")) {
						try { x = py::float_(data["time/total_timesteps"]).cast<double>(); }
						catch (...) {}
					}

					std::lock_guard<std::mutex> lock(metricsMutex);
					if (x < 0.0) x = static_cast<double>(metricStepCounter++);

					for (auto item : data) {
						std::string key = py::str(item.first).cast<std::string>();
						double value;
						try { value = item.second.cast<double>(); }
						catch (...) { continue; }
						metricSeries[key].AddPoint((float)x, (float)value);
					}
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

		Console::Print("[Training] Training finished.");
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
	pendingEnd.exchange(false);

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
	ImGui::Indent(8.0f);

	if (ImGui::BeginTabBar("##HeadlessMonitorTabs")) {
		if (ImGui::BeginTabItem("Training Monitor")) {
			DrawTrainingMonitorTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Console")) {
			headlessConsole.DrawContent();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::Unindent(8.0f);
	ImGui::End();
}

void HeadlessMonitor::DrawTrainingMonitorTab() {
	bool isTraining = IsTraining();

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::TextColored(isTraining ? ImVec4(0.35f, 0.85f, 0.4f, 1.0f) : ImVec4(0.85f, 0.35f, 0.35f, 1.0f),
		isTraining ? "Training" : "Finished");

	if (isTraining) {
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "%s", GetTrainingStatus().c_str());
	}
	else {
		ImGui::SameLine(0.0f, 12.0f);
		if (ImGui::Button("Back to Editor")) {
			End();
		}
	}

	std::string trainErr = GetTrainingError();
	if (!trainErr.empty()) {
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
		ImGui::TextWrapped("Training failed: %s", trainErr.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Dummy(ImVec2(0, 8));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 8));

	std::lock_guard<std::mutex> lock(metricsMutex);

	if (metricSeries.empty()) {
		ImGui::TextDisabled(isTraining ? "Waiting for first rollout..." : "No training metrics recorded.");
		return;
	}

	std::map<std::string, std::vector<std::string>> sections;
	for (auto& [key, buf] : metricSeries) {
		size_t slash = key.find('/');
		std::string section = (slash != std::string::npos) ? key.substr(0, slash) : "misc";
		sections[section].push_back(key);
	}

	const float plotWidth = ImGui::GetContentRegionAvail().x * 0.48f;
	const ImVec2 plotSize(plotWidth, 150.0f);

	for (auto& [section, keys] : sections) {
		std::string headerLabel = section + "/";
		if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			int col = 0;
			for (auto& key : keys) {
				MetricBuffer& buf = metricSeries[key];
				if (buf.data.empty()) continue;

				std::string label = key;
				size_t slash = key.find('/');
				if (slash != std::string::npos) label = key.substr(slash + 1);

				if (col % 2 != 0) ImGui::SameLine();
				col++;

				ImGui::BeginGroup();
				ImGui::Text("%s: %.4g", label.c_str(), buf.data.back().y);

				std::string plotId = "##plot_" + key;
				if (ImPlot::BeginPlot(plotId.c_str(), plotSize,
					ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus)) {
					ImPlot::SetupAxes("timesteps", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

					ImPlotSpec spec;
					spec.Offset = buf.offset;
					spec.Stride = sizeof(ImVec2);

					ImPlot::PlotLine(label.c_str(),
						&buf.data[0].x, &buf.data[0].y,
						(int)buf.data.size(), spec);

					ImPlot::EndPlot();
				}
				ImGui::EndGroup();
			}
		}
	}
}