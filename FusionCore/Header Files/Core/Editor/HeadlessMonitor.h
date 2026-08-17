#pragma once
#include <string>
#include "Windows/Console.h"

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

	void Start();
	void Stop();
	bool IsRunning() const;

	void StartTraining(const TrainConfig& config);
	bool IsTraining() const { return training.load(); }
	std::string GetTrainingStatus() const;
	std::string GetTrainingError() const;

	void ProcessMonitorWindow();

private:
	HeadlessMonitor() = default;
	~HeadlessMonitor();

	Console headlessConsole{ "Headless Console" };

	std::string projectDisplayName;
	bool autoScroll = true;

	std::atomic<bool> training{ false };
	std::thread trainingThread;

	mutable std::mutex trainStatusMutex;
	std::string trainStatus;
	std::string trainError;
};