#pragma once
#include <string>
#include "Windows/Console.h"
#include "../Files/BinaryReader.h"
#include "../Files/BinaryWriter.h"

struct TrainConfig {
	long long totalTimesteps = 100000;
	std::string algorithm = "PPO"; // PPO | SAC | A2C | DDPG | TD3
	std::string saveDir;
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

	void Begin(); 
	void End();   

	Console headlessConsole{ "Headless Console" };

	std::string projectDisplayName;
	bool autoScroll = true;

	std::atomic<bool> training{ false };
	std::atomic<bool> pendingEnd{ false };
	std::thread trainingThread;

	mutable std::mutex trainStatusMutex;
	std::string trainStatus;
	std::string trainError;
};