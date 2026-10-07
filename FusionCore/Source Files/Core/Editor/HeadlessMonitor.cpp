#include "../../../Header Files/Core/Editor/HeadlessMonitor.h"
#include "../../../Header Files/Core/EngineManager.h"
#include "../../../Header Files/Core/SceneManager.h"
#include "../../../Header Files/Core/Files/FileManager.h"
#include "../../../Header Files/Core/Scripting/ScriptManager.h"
#include "../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../Header Files/Core/Camera.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <filesystem>
#include <pybind11/embed.h>
#include <GLFW/glfw3.h>

namespace py = pybind11;

namespace {
	void StatusPill(const char* text, const ImVec4& color, bool pulse) {
		const ImGuiStyle& st = ImGui::GetStyle();
		const float h = ImGui::GetFrameHeight();
		const float dotR = 4.0f;
		const ImVec2 ts = ImGui::CalcTextSize(text);
		const float w = ts.x + dotR * 2.0f + 28.0f;

		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.16f)), h * 0.5f);

		float alpha = 1.0f;
		if (pulse) alpha = 0.55f + 0.45f * (0.5f + 0.5f * sinf((float)ImGui::GetTime() * 4.0f));
		dl->AddCircleFilled(ImVec2(p.x + 14.0f, p.y + h * 0.5f), dotR, ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, alpha)));
		dl->AddText(ImVec2(p.x + 14.0f + dotR + 8.0f, p.y + (h - ts.y) * 0.5f), ImGui::GetColorU32(color), text);
		ImGui::Dummy(ImVec2(w, h));
	}

	float StatusPillWidth(const char* text) {
		return ImGui::CalcTextSize(text).x + 8.0f + 28.0f;
	}

	void Banner(const char* id, const ImVec4& color, const char* title, const std::string& text) {
		const ImGuiStyle& st = ImGui::GetStyle();
		const float pad = 12.0f;
		const float wrapW = ImGui::GetContentRegionAvail().x - pad * 2.0f;
		const ImVec2 ts = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapW);
		const float h = pad * 2.0f + ImGui::GetTextLineHeight() + st.ItemSpacing.y + ts.y;

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(color.x, color.y, color.z, 0.10f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(color.x, color.y, color.z, 0.45f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
		ImGui::BeginChild(id, ImVec2(0, h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
		EditorField::BoldText(title, &color);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
		ImGui::TextWrapped("%s", text.c_str());
		ImGui::PopStyleColor();
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
	}
}

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
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoSavedSettings;

	const float kPad = 24.0f;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPad, 18.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##HeadlessMonitor", nullptr, flags);
	ImGui::PopStyleVar(2);

	const ImGuiStyle& st = ImGui::GetStyle();
	const bool isTraining = IsTraining();
	const bool stopping = IsStopRequested();
	const bool failed = !GetTrainingError().empty();

	{
		const float rowTop = ImGui::GetCursorPosY();

		ImGui::SetWindowFontScale(1.4f);
		EditorField::BoldText("Training");
		ImGui::SetWindowFontScale(1.0f);
		const float rowH = ImGui::GetItemRectSize().y;

		ImGui::SameLine(0.0f, 12.0f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", projectDisplayName.c_str());

		const char* pillText = isTraining ? (stopping ? "Stopping" : "Training") : (failed ? "Failed" : "Finished");
		const ImVec4 pillColor = isTraining ? (stopping ? ImVec4(0.95f, 0.65f, 0.25f, 1.0f) : EditorTheme::Accent())
			: (failed ? EditorTheme::Danger() : ImVec4(0.65f, 0.67f, 0.70f, 1.0f));

		const char* btnText = isTraining ? (stopping ? "Stopping..." : "Stop & Save") : "Back to Editor";
		const float btnW = ImGui::CalcTextSize(btnText).x + st.FramePadding.x * 2.0f + 16.0f;
		const float total = StatusPillWidth(pillText) + st.ItemSpacing.x + btnW;

		ImGui::SameLine(ImGui::GetWindowWidth() - total - kPad);
		ImGui::SetCursorPosY(rowTop + (rowH - ImGui::GetFrameHeight()) * 0.5f);
		StatusPill(pillText, pillColor, isTraining && !stopping);

		ImGui::SameLine();
		if (isTraining) {
			ImGui::BeginDisabled(stopping);
			if (EditorField::DangerButton(btnText, ImVec2(btnW, 0))) RequestStop();
			ImGui::EndDisabled();
		}
		else {
			if (EditorField::AddButton(btnText)) End();
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
	if (ImGui::BeginTabBar("##HeadlessMonitorTabs")) {
		if (ImGui::BeginTabItem("Training Monitor")) {
			ImGui::PopStyleVar();
			ImGui::Spacing();
			DrawTrainingMonitorTab();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Live View")) {
			EngineManager::getInstance().liveTrainingRenderActive = isTraining;
			ImGui::PopStyleVar();
			ImGui::Spacing();
			DrawLiveTrainingViewTab();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Console")) {
			ImGui::PopStyleVar();
			ImGui::Spacing();
			headlessConsole.DrawContent();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::PopStyleVar();

	ImGui::End();
}

void HeadlessMonitor::DrawTrainingMonitorTab() {
	const ImGuiStyle& st = ImGui::GetStyle();
	const bool isTraining = IsTraining();

	if (isTraining) {
		ImGui::TextDisabled("%s", GetTrainingStatus().c_str());
		ImGui::Spacing();
	}

	const std::string trainErr = GetTrainingError();
	if (!trainErr.empty()) {
		Banner("##trainErr", EditorTheme::Danger(), "Training failed", trainErr);
		ImGui::Spacing();
	}

	std::lock_guard<std::mutex> lock(metricsMutex);

	if (metricSeries.empty()) {
		ImGui::Dummy(ImVec2(0, 40.0f));
		const char* msg = isTraining ? "Waiting for the first rollout..." : "No training metrics were recorded.";
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(msg).x) * 0.5f);
		ImGui::TextDisabled("%s", msg);
		return;
	}

	std::map<std::string, std::vector<std::string>> sections;
	for (auto& [key, buf] : metricSeries) {
		size_t slash = key.find('/');
		std::string section = (slash != std::string::npos) ? key.substr(0, slash) : "misc";
		sections[section].push_back(key);
	}

	static const ImVec4 kLineColors[] = {
		ImVec4(0.36f, 0.70f, 0.62f, 1.0f),
		ImVec4(0.45f, 0.62f, 0.88f, 1.0f),
		ImVec4(0.88f, 0.68f, 0.38f, 1.0f),
		ImVec4(0.72f, 0.55f, 0.85f, 1.0f),
	};

	const float avail = ImGui::GetContentRegionAvail().x;
	const float minCardW = 380.0f;
	const int cols = std::max(1, (int)((avail + st.ItemSpacing.x) / (minCardW + st.ItemSpacing.x)));
	const float cardW = (avail - st.ItemSpacing.x * (cols - 1)) / (float)cols;
	const float cardH = 210.0f;

	int sectionIdx = 0;
	for (auto& [section, keys] : sections) {
		const ImVec4 lineColor = kLineColors[sectionIdx++ % 4];

		std::string header = section + "##section_" + section;
		bool bold = EditorTheme::PushBold();
		const bool open = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		EditorTheme::PopBold(bold);
		if (!open) continue;

		ImGui::Spacing();
		int col = 0;
		for (auto& key : keys) {
			MetricBuffer& buf = metricSeries[key];
			if (buf.data.empty()) continue;

			std::string label = key;
			size_t slash = key.find('/');
			if (slash != std::string::npos) label = key.substr(slash + 1);

			if (col % cols != 0) ImGui::SameLine();
			col++;

			ImGui::BeginChild(("##card_" + key).c_str(), ImVec2(cardW, cardH), ImGuiChildFlags_Borders,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

			char valueText[32];
			const float last = buf.data.back().y;
			if (std::abs(last) >= 1000.0f) std::snprintf(valueText, sizeof(valueText), "%.0f", last);
			else std::snprintf(valueText, sizeof(valueText), "%.4g", last);

			EditorField::BoldText(label.c_str());
			ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(valueText).x - st.WindowPadding.x);
			ImGui::PushStyleColor(ImGuiCol_Text, lineColor);
			ImGui::TextUnformatted(valueText);
			ImGui::PopStyleColor();

			ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0, 0, 0, 0));
			ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			ImPlot::PushStyleColor(ImPlotCol_PlotBorder, ImGui::GetStyleColorVec4(ImGuiCol_Border));
			ImPlot::PushStyleColor(ImPlotCol_AxisGrid, ImVec4(1, 1, 1, 0.06f));
			ImPlot::PushStyleColor(ImPlotCol_AxisText, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

			if (ImPlot::BeginPlot(("##plot_" + key).c_str(), ImVec2(-1, -1),
				ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus)) {
				ImPlot::SetupAxes("timesteps", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

				ImPlotSpec spec;
				spec.Offset = buf.offset;
				spec.Stride = sizeof(ImVec2);
				spec.LineColor = lineColor;
				spec.LineWeight = 2.0f;

				ImPlot::PlotLine(label.c_str(), &buf.data[0].x, &buf.data[0].y, (int)buf.data.size(), spec);
				ImPlot::EndPlot();
			}
			ImPlot::PopStyleColor(5);

			ImGui::EndChild();
		}
		ImGui::Spacing();
	}
}

void HeadlessMonitor::DrawLiveTrainingViewTab() {
	Banner("##LiveViewWarning", ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "Note",
		"This view renders the agent in real time so you can watch its behavior. Rendering every step can slow "
		"training down, and the slightly different timing may affect results. For the fastest, most consistent "
		"training, stay on the Training Monitor or Console tab and check in here only occasionally.");
	ImGui::Spacing();

	Viewport* gameViewport = EditorManager::getInstance().gameViewport;
	if (!IsTraining() || !gameViewport) {
		ImGui::Dummy(ImVec2(0, 40.0f));
		const char* msg = "Training isn't running, so there is nothing to display.";
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(msg).x) * 0.5f);
		ImGui::TextDisabled("%s", msg);
		return;
	}

	const float aspect = (float)gameViewport->textureWidth / (float)gameViewport->textureHeight;
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	float w = avail.x;
	float h = w / aspect;
	if (h > avail.y) { h = avail.y; w = h * aspect; }

	const ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - w) * 0.5f, cursor.y + (avail.y - h) * 0.5f));

	const ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::Image((ImTextureID)(intptr_t)gameViewport->colorTexture, ImVec2(w, h), ImVec2(0, 1), ImVec2(1, 0));
	ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
}