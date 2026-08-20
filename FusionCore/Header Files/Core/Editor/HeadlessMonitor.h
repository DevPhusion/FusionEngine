#pragma once
#include <string>
#include "Windows/Console.h"
#include "../Files/BinaryReader.h"
#include "../Files/BinaryWriter.h"
#include "../../../imgui/implot.h"
#include <map>

struct TrainConfig {
	long long totalTimesteps = 100000;
	std::string algorithm = "PPO"; // PPO | SAC | A2C | DDPG | TD3
	std::string saveDir;
	std::string startFromModelPath;
};

struct MetricBuffer {
	std::vector<ImVec2> data;
	int offset = 0;
	int maxSize = 2000;

	void AddPoint(float x, float y) {
		if ((int)data.size() < maxSize) {
			data.push_back(ImVec2(x, y));
		}
		else {
			data[offset] = ImVec2(x, y);
			offset = (offset + 1) % maxSize;
		}
	}
};

class HeadlessMonitor {
public:
	static HeadlessMonitor& getInstance() {
		static HeadlessMonitor instance;
		return instance;
	}

	HeadlessMonitor(const HeadlessMonitor&) = delete;
	HeadlessMonitor& operator=(const HeadlessMonitor&) = delete;

	TrainConfig config;

	void StartTraining(const TrainConfig& config);
	bool IsTraining() const { return training.load(); }
	std::string GetTrainingStatus() const;
	std::string GetTrainingError() const;

	void ProcessMonitorWindow();

	void SerializeTrainConfig(BinaryWriter& w);
	void DeserializeTrainConfig(BinaryReader& r);

private:
	HeadlessMonitor() = default;
	~HeadlessMonitor();   

	std::mutex metricsMutex;
	std::map<std::string, MetricBuffer> metricSeries;
	uint64_t metricStepCounter = 0;

	Console headlessConsole{ "Headless Console" };

	std::string projectDisplayName;

	std::atomic<bool> training{ false };
	std::atomic<bool> pendingEnd{ false };
	std::thread trainingThread;

	mutable std::mutex trainStatusMutex;
	std::string trainStatus;
	std::string trainError;

	void Begin();
	void End();
	void DrawTrainingMonitorTab();
};