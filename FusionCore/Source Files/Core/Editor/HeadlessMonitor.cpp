#include "../../../Header Files/Core/Editor/HeadlessMonitor.h"
#include "../../../Header Files/Core/EngineManager.h"
#include "../../../Header Files/Core/SceneManager.h"
#include "../../../Header Files/Core/Files/FileManager.h"
#include "../../../Header Files/Core/Scripting/ScriptManager.h"
#include "../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../Header Files/Core/Camera.h"
#include <filesystem>
#include <pybind11/embed.h>
#include <GLFW/glfw3.h>

namespace py = pybind11;

HeadlessMonitor::~HeadlessMonitor() {
	if (trainingThread.joinable())
		trainingThread.join();
}

void HeadlessMonitor::SerializeTrainConfig(BinaryWriter& w) {
	w.Write(config.totalTimesteps);
	w.WriteString(config.algorithm);
	w.WriteString(config.policy);
	w.WriteString(config.modelName);
	w.WriteString(config.saveDir);
	w.WriteString(config.startFromModelPath);
	w.Write(config.shardIntervalSteps);
	w.WriteString(config.shardDir);

	auto& ppo = config.ppoSettings;
	w.Write(ppo.learningRate); w.Write(ppo.nSteps); w.Write(ppo.batchSize);
	w.Write(ppo.nEpochs); w.Write(ppo.gamma); w.Write(ppo.gaeLambda);
	w.Write(ppo.clipRange); w.Write(ppo.entCoef); w.Write(ppo.vfCoef);
	w.Write(ppo.maxGradNorm);

	auto& a2c = config.a2cSettings;
	w.Write(a2c.learningRate); w.Write(a2c.nSteps); w.Write(a2c.gamma);
	w.Write(a2c.gaeLambda); w.Write(a2c.entCoef); w.Write(a2c.vfCoef);
	w.Write(a2c.maxGradNorm);

	auto& sac = config.sacSettings;
	w.Write(sac.learningRate); w.Write(sac.bufferSize); w.Write(sac.learningStarts);
	w.Write(sac.batchSize); w.Write(sac.tau); w.Write(sac.gamma);
	w.Write(sac.trainFreq); w.Write(sac.gradientSteps);

	auto& ddpg = config.ddpgSettings;
	w.Write(ddpg.learningRate); w.Write(ddpg.bufferSize); w.Write(ddpg.learningStarts);
	w.Write(ddpg.batchSize); w.Write(ddpg.tau); w.Write(ddpg.gamma);
	w.Write(ddpg.trainFreq); w.Write(ddpg.gradientSteps);

	auto& td3 = config.td3Settings;
	w.Write(td3.learningRate); w.Write(td3.bufferSize); w.Write(td3.learningStarts);
	w.Write(td3.batchSize); w.Write(td3.tau); w.Write(td3.gamma);
	w.Write(td3.trainFreq); w.Write(td3.gradientSteps); w.Write(td3.policyDelay);
	w.Write(td3.targetPolicyNoise); w.Write(td3.targetNoiseClip);
}

void HeadlessMonitor::DeserializeTrainConfig(BinaryReader& r) {
	config.totalTimesteps = r.Read<long long>();
	config.algorithm = r.ReadString();
	config.policy = r.ReadString();
	config.modelName = r.ReadString();
	config.saveDir = r.ReadString();
	config.startFromModelPath = r.ReadString();
	config.shardIntervalSteps = r.Read<int>();
	config.shardDir = r.ReadString();

	auto& ppo = config.ppoSettings;
	ppo.learningRate = r.Read<float>(); ppo.nSteps = r.Read<int>(); ppo.batchSize = r.Read<int>();
	ppo.nEpochs = r.Read<int>(); ppo.gamma = r.Read<float>(); ppo.gaeLambda = r.Read<float>();
	ppo.clipRange = r.Read<float>(); ppo.entCoef = r.Read<float>(); ppo.vfCoef = r.Read<float>();
	ppo.maxGradNorm = r.Read<float>();

	auto& a2c = config.a2cSettings;
	a2c.learningRate = r.Read<float>(); a2c.nSteps = r.Read<int>(); a2c.gamma = r.Read<float>();
	a2c.gaeLambda = r.Read<float>(); a2c.entCoef = r.Read<float>(); a2c.vfCoef = r.Read<float>();
	a2c.maxGradNorm = r.Read<float>();

	auto& sac = config.sacSettings;
	sac.learningRate = r.Read<float>(); sac.bufferSize = r.Read<int>(); sac.learningStarts = r.Read<int>();
	sac.batchSize = r.Read<int>(); sac.tau = r.Read<float>(); sac.gamma = r.Read<float>();
	sac.trainFreq = r.Read<int>(); sac.gradientSteps = r.Read<int>();

	auto& ddpg = config.ddpgSettings;
	ddpg.learningRate = r.Read<float>(); ddpg.bufferSize = r.Read<int>(); ddpg.learningStarts = r.Read<int>();
	ddpg.batchSize = r.Read<int>(); ddpg.tau = r.Read<float>(); ddpg.gamma = r.Read<float>();
	ddpg.trainFreq = r.Read<int>(); ddpg.gradientSteps = r.Read<int>();

	auto& td3 = config.td3Settings;
	td3.learningRate = r.Read<float>(); td3.bufferSize = r.Read<int>(); td3.learningStarts = r.Read<int>();
	td3.batchSize = r.Read<int>(); td3.tau = r.Read<float>(); td3.gamma = r.Read<float>();
	td3.trainFreq = r.Read<int>(); td3.gradientSteps = r.Read<int>(); td3.policyDelay = r.Read<int>();
	td3.targetPolicyNoise = r.Read<float>(); td3.targetNoiseClip = r.Read<float>();
}

py::dict HeadlessMonitor::BuildHyperparams() {
	py::dict hp;

	if (config.algorithm == "PPO") {
		auto& s = config.ppoSettings;
		hp["learning_rate"] = s.learningRate;
		hp["n_steps"] = s.nSteps;
		hp["batch_size"] = s.batchSize;
		hp["n_epochs"] = s.nEpochs;
		hp["gamma"] = s.gamma;
		hp["gae_lambda"] = s.gaeLambda;
		hp["clip_range"] = s.clipRange;
		hp["ent_coef"] = s.entCoef;
		hp["vf_coef"] = s.vfCoef;
		hp["max_grad_norm"] = s.maxGradNorm;
	}
	else if (config.algorithm == "A2C") {
		auto& s = config.a2cSettings;
		hp["learning_rate"] = s.learningRate;
		hp["n_steps"] = s.nSteps;
		hp["gamma"] = s.gamma;
		hp["gae_lambda"] = s.gaeLambda;
		hp["ent_coef"] = s.entCoef;
		hp["vf_coef"] = s.vfCoef;
		hp["max_grad_norm"] = s.maxGradNorm;
	}
	else if (config.algorithm == "SAC") {
		auto& s = config.sacSettings;
		hp["learning_rate"] = s.learningRate;
		hp["buffer_size"] = s.bufferSize;
		hp["learning_starts"] = s.learningStarts;
		hp["batch_size"] = s.batchSize;
		hp["tau"] = s.tau;
		hp["gamma"] = s.gamma;
		hp["train_freq"] = s.trainFreq;
		hp["gradient_steps"] = s.gradientSteps;
	}
	else if (config.algorithm == "DDPG") {
		auto& s = config.ddpgSettings;
		hp["learning_rate"] = s.learningRate;
		hp["buffer_size"] = s.bufferSize;
		hp["learning_starts"] = s.learningStarts;
		hp["batch_size"] = s.batchSize;
		hp["tau"] = s.tau;
		hp["gamma"] = s.gamma;
		hp["train_freq"] = s.trainFreq;
		hp["gradient_steps"] = s.gradientSteps;
	}
	else if (config.algorithm == "TD3") {
		auto& s = config.td3Settings;
		hp["learning_rate"] = s.learningRate;
		hp["buffer_size"] = s.bufferSize;
		hp["learning_starts"] = s.learningStarts;
		hp["batch_size"] = s.batchSize;
		hp["tau"] = s.tau;
		hp["gamma"] = s.gamma;
		hp["train_freq"] = s.trainFreq;
		hp["gradient_steps"] = s.gradientSteps;
		hp["policy_delay"] = s.policyDelay;
		hp["target_policy_noise"] = s.targetPolicyNoise;
		hp["target_noise_clip"] = s.targetNoiseClip;
	}

	return hp;
}

void HeadlessMonitor::Begin() {
	projectDisplayName = std::filesystem::path(FileManager::getInstance().currentProjectFile).filename().string();

	SceneManager& SM = SceneManager::getInstance();

	EngineManager::getInstance().editingScenePath = SM.GetCurrentSceneFile();

	const std::string& mainScene = EngineManager::getInstance().EngineSettings.mainScenePath;
	std::error_code ec;
	if (!mainScene.empty() && std::filesystem::exists(mainScene, ec) && !ec) {
		trainingScenePath = mainScene;
	}
	else {
		Console::PrintWarning("[Training] No valid main scene set in Settings; training on the currently open scene instead.");
		trainingScenePath = SM.GetCurrentSceneFile();
	}

	SM.LoadSceneFromFile(trainingScenePath);

	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
	EngineManager::getInstance().isHeadless = true;
	liveTabActiveLastFrame = false;

	Renderer::getInstance().ResetSnapshotSceneReloadFlag();

	Renderer::getInstance().CaptureSnapshot(4, 4);

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
	stopRequested.store(false);
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

			std::string shardDir = config.shardDir;
			if (config.shardIntervalSteps > 0 && shardDir.empty()) {
				shardDir = (std::filesystem::path(saveDir) / "Shards").string();
			}

			std::string startFromAbsPath;
			if (!config.startFromModelPath.empty()) {
				startFromAbsPath = FileManager::getInstance()
					.VirtualToAbsolute(config.startFromModelPath).string();
			}

			std::string modelName = config.modelName.empty() ? "trained_model" : config.modelName;
			py::dict hyperparams = BuildHyperparams();

			trainFn(config.algorithm, config.policy, config.totalTimesteps, saveDir, startFromAbsPath, modelName, hyperparams,
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
					}),
				py::cpp_function([this]() { return stopRequested.load(); }),
				config.shardIntervalSteps,
				shardDir);
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
		stopRequested.store(false);
		});
}

void HeadlessMonitor::RequestStop() {
	if (!training.load()) return;
	stopRequested.store(true);
	{
		std::lock_guard<std::mutex> lock(trainStatusMutex);
		trainStatus = "Stop requested — finishing current step and saving...";
	}
	Console::Print("[Training] Stop requested by user.");
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
	EngineManager::getInstance().liveTrainingRenderActive = false;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
	ImGui::Begin("##HeadlessMonitor", nullptr, flags);
	ImGui::PopStyleVar();

	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont());
	ImGui::TextColored(ImVec4(0.92f, 0.92f, 0.95f, 1.0f), "Training");
	ImGui::PopFont();
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "- %s", projectDisplayName.c_str());

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 6));

	ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.13f, 0.14f, 0.17f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.18f, 0.30f, 0.44f, 1.0f));

	bool liveTabActiveThisFrame = false;

	if (ImGui::BeginTabBar("##HeadlessMonitorTabs")) {
		if (ImGui::BeginTabItem("Training Monitor")) {
			DrawTrainingMonitorTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Live Training View")) {
			liveTabActiveThisFrame = true;
			EngineManager::getInstance().liveTrainingRenderActive = IsTraining();

			if (!liveTabActiveLastFrame && IsTraining()) {
				RefreshLiveViewScene();
			}

			DrawLiveTrainingViewTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Console")) {
			headlessConsole.DrawContent();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::PopStyleColor(3);

	liveTabActiveLastFrame = liveTabActiveThisFrame;

	ImGui::End();
}

void HeadlessMonitor::DrawTrainingMonitorTab() {
	bool isTraining = IsTraining();

	ImGui::Dummy(ImVec2(0, 8));

	ImVec4 statusColor = isTraining ? ImVec4(0.30f, 0.80f, 0.45f, 1.0f) : ImVec4(0.85f, 0.40f, 0.40f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(statusColor.x, statusColor.y, statusColor.z, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
	ImGui::Button(isTraining ? "  Training  " : "  Finished  ");
	ImGui::PopStyleColor(2);

	if (isTraining) {
		ImGui::SameLine(0.0f, 14.0f);
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "%s", GetTrainingStatus().c_str());

		ImGui::SameLine(0.0f, 14.0f);
		bool stopping = IsStopRequested();
		ImGui::BeginDisabled(stopping);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.22f, 0.22f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.28f, 0.28f, 1.0f));
		if (ImGui::Button(stopping ? "Stopping..." : "Stop && Save")) {
			RequestStop();
		}
		ImGui::PopStyleColor(2);
		ImGui::EndDisabled();
	}
	else {
		ImGui::SameLine(0.0f, 14.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.42f, 0.60f, 1.0f));
		if (ImGui::Button("Back to Editor")) {
			End();
		}
		ImGui::PopStyleColor(2);
	}

	std::string trainErr = GetTrainingError();
	if (!trainErr.empty()) {
		ImGui::Dummy(ImVec2(0, 8));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.30f, 0.10f, 0.10f, 0.35f));
		ImGui::BeginChild("##trainErr", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.55f, 1.0f));
		ImGui::TextWrapped("Training failed: %s", trainErr.c_str());
		ImGui::PopStyleColor();
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	ImGui::Dummy(ImVec2(0, 10));

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

	const float plotWidth = ImGui::GetContentRegionAvail().x * 0.5f - 16.0f;
	const ImVec2 plotSize(plotWidth, 140.0f);

	static const ImVec4 kAccents[] = {
		ImVec4(0.35f, 0.65f, 0.95f, 1.0f), 
		ImVec4(0.55f, 0.85f, 0.55f, 1.0f), 
		ImVec4(0.90f, 0.70f, 0.35f, 1.0f),
		ImVec4(0.80f, 0.55f, 0.90f, 1.0f), 
	};
	int sectionIdx = 0;

	for (auto& [section, keys] : sections) {
		ImVec4 accent = kAccents[sectionIdx % 4];
		sectionIdx++;

		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(accent.x, accent.y, accent.z, 0.12f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, 0.20f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accent.x, accent.y, accent.z, 0.25f));
		ImGui::PushStyleColor(ImGuiCol_Text, accent);

		std::string headerLabel = section + "/";
		bool open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		ImGui::PopStyleColor(4);

		if (open) {
			ImGui::Dummy(ImVec2(0, 4));
			int col = 0;
			for (auto& key : keys) {
				MetricBuffer& buf = metricSeries[key];
				if (buf.data.empty()) continue;

				std::string label = key;
				size_t slash = key.find('/');
				if (slash != std::string::npos) label = key.substr(slash + 1);

				if (col % 2 != 0) ImGui::SameLine(0.0f, 16.0f);
				col++;

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 0.03f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
				ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

				ImGui::BeginChild(("##card_" + key).c_str(), ImVec2(plotSize.x + 20, plotSize.y + 50), true);

				ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "%s", label.c_str());
				ImGui::SameLine();
				if (std::abs(buf.data.back().y) >= 1000.0f)
					ImGui::TextColored(accent, "%.0f", buf.data.back().y);
				else
					ImGui::TextColored(accent, "%.4g", buf.data.back().y);

				ImGui::Dummy(ImVec2(0, 4));

				ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
				ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
				ImPlot::PushStyleColor(ImPlotCol_PlotBorder, ImVec4(1, 1, 1, 0.06f));
				ImPlot::PushStyleColor(ImPlotCol_AxisGrid, ImVec4(1, 1, 1, 0.06f));
				ImPlot::PushStyleColor(ImPlotCol_AxisText, ImVec4(0.65f, 0.68f, 0.72f, 1.0f));
				ImPlot::PushStyleColor(ImPlotCol_AxisBg, ImVec4(0, 0, 0, 0));
				ImPlot::PushStyleColor(ImPlotCol_AxisBgHovered, ImVec4(1, 1, 1, 0.06f));
				ImPlot::PushStyleColor(ImPlotCol_AxisBgActive, ImVec4(1, 1, 1, 0.10f));

				std::string plotId = "##plot_" + key;
				if (ImPlot::BeginPlot(plotId.c_str(), plotSize,
					ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus)) {
					ImPlot::SetupAxes("timesteps", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

					ImPlotSpec spec;
					spec.Offset = buf.offset;
					spec.Stride = sizeof(ImVec2);
					spec.LineColor = accent;
					spec.LineWeight = 2.0f;

					ImPlot::PlotLine(label.c_str(),
						&buf.data[0].x, &buf.data[0].y,
						(int)buf.data.size(), spec);

					ImPlot::EndPlot();
				}
				ImPlot::PopStyleColor(8);

				ImGui::EndChild();

				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor(2);
			}
			ImGui::Dummy(ImVec2(0, 8));
		}
	}
}

void HeadlessMonitor::DrawLiveTrainingViewTab() {
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.04f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.25f, 1.0f));

	bool warningOpen = ImGui::CollapsingHeader("Warning##LiveViewWarningHeader");

	ImGui::PopStyleColor(4);

	if (warningOpen) {
		const char* warningText =
			"This view renders the agent's training in real time so you can watch its behavior. "
			"Rendering every step can slow down training and, because timing/state differs slightly "
			"from headless stepping, may also affect training results. For fastest and most consistent "
			"training, prefer running headless (stay on the Training Monitor or Console tab) and only "
			"check in here occasionally.";

		const float horizontalPadding = 24.0f;
		const float verticalPadding = 16.0f;
		float wrapWidth = ImGui::GetContentRegionAvail().x - horizontalPadding;
		ImVec2 textSize = ImGui::CalcTextSize(warningText, nullptr, false, wrapWidth);
		float boxHeight = textSize.y + verticalPadding;

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.45f, 0.34f, 0.05f, 0.18f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.65f, 0.15f, 0.55f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));

		ImGui::BeginChild("##LiveViewWarningBox", ImVec2(0, boxHeight), true, ImGuiWindowFlags_NoScrollbar);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.25f, 1.0f));
		ImGui::TextWrapped("%s", warningText);
		ImGui::PopStyleColor();

		ImGui::EndChild();

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(2);
	}

	ImGui::Dummy(ImVec2(0, 8));

	if (!IsTraining()) {
		ImGui::TextDisabled("Training isn't running -- nothing to display.");
		return;
	}

	Viewport* gameViewport = EditorManager::getInstance().gameViewport;
	GLuint tex = gameViewport->colorTexture;

	float aspect = (float)gameViewport->textureWidth / (float)gameViewport->textureHeight;
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float displayWidth = avail.x;
	float displayHeight = displayWidth / aspect;
	if (displayHeight > avail.y) { displayHeight = avail.y; displayWidth = displayHeight * aspect; }

	ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(
		cursor.x + (avail.x - displayWidth) * 0.5f,
		cursor.y + (avail.y - displayHeight) * 0.5f
	));

	ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(displayWidth, displayHeight), ImVec2(0, 1), ImVec2(1, 0));
}

void HeadlessMonitor::RefreshLiveViewScene() {
	std::lock_guard<std::mutex> lock(EngineManager::getInstance().headlessSimMutex);
	ReloadTrainingScene();
}

void HeadlessMonitor::ReloadTrainingScene() {
	if (!trainingScenePath.empty()) {
		SceneManager::getInstance().LoadSceneFromFile(trainingScenePath);
	}
	else {
		SceneManager::getInstance().NewScene();
	}

	ScriptManager::getInstance().RunAllScriptsLoad();
	ScriptManager::getInstance().RunAllScriptsStart();
}

void HeadlessMonitor::RequestSceneReload() {
	if (trainingScenePath.empty()) {
		Console::PrintWarning("[Training] RequestSceneReload: no training scene path set, skipping.");
		return;
	}
	SceneManager::getInstance().RequestLoadScene(trainingScenePath);
}