#pragma once
#include <string>
#include "Windows/Console.h"
#include "../Files/BinaryReader.h"
#include "../Files/BinaryWriter.h"
#include "../../../imgui/implot.h"
#include <pybind11/pybind11.h>
#include <map>

struct PPOAdvancedSettings {
	float learningRate = 3e-4f;
	int   nSteps = 2048;
	int   batchSize = 64;
	int   nEpochs = 10;
	float gamma = 0.99f;
	float gaeLambda = 0.95f;
	float clipRange = 0.2f;
	float entCoef = 0.0f;
	float vfCoef = 0.5f;
	float maxGradNorm = 0.5f;
};

struct A2CAdvancedSettings {
	float learningRate = 7e-4f;
	int   nSteps = 5;
	float gamma = 0.99f;
	float gaeLambda = 1.0f;
	float entCoef = 0.0f;
	float vfCoef = 0.5f;
	float maxGradNorm = 0.5f;
};

struct SACAdvancedSettings {
	float learningRate = 3e-4f;
	int   bufferSize = 1000000;
	int   learningStarts = 100;
	int   batchSize = 256;
	float tau = 0.005f;
	float gamma = 0.99f;
	int   trainFreq = 1;
	int   gradientSteps = 1;
};

struct DDPGAdvancedSettings {
	float learningRate = 1e-3f;
	int   bufferSize = 1000000;
	int   learningStarts = 100;
	int   batchSize = 256;
	float tau = 0.005f;
	float gamma = 0.99f;
	int   trainFreq = 1;
	int   gradientSteps = 1;
};

struct TD3AdvancedSettings {
	float learningRate = 1e-3f;
	int   bufferSize = 1000000;
	int   learningStarts = 100;
	int   batchSize = 256;
	float tau = 0.005f;
	float gamma = 0.99f;
	int   trainFreq = 1;
	int   gradientSteps = 1;
	int   policyDelay = 2;
	float targetPolicyNoise = 0.2f;
	float targetNoiseClip = 0.5f;
};

struct TrainConfig {
	long long totalTimesteps = 100000;
	std::string algorithm = "PPO"; // PPO | SAC | A2C | DDPG | TD3
	std::string policy = "MlpPolicy"; //MlpPolicy | CnnPolicy | MultiInputPolicy
	std::string modelName = "trained_model"; 
	std::string saveDir;
	std::string startFromModelPath;
	int shardIntervalSteps = 0;   
	std::string shardDir;

	PPOAdvancedSettings  ppoSettings;
	A2CAdvancedSettings  a2cSettings;
	SACAdvancedSettings  sacSettings;
	DDPGAdvancedSettings ddpgSettings;
	TD3AdvancedSettings  td3Settings;
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
	std::string trainingScenePath;

	void StartTraining(const TrainConfig& config);
	bool IsTraining() const { return training.load(); }
	std::string GetTrainingStatus() const;
	std::string GetTrainingError() const;

	void RequestStop();
	bool IsStopRequested() const { return stopRequested.load(); }

	void ProcessMonitorWindow();

	void SerializeTrainConfig(BinaryWriter& w);
	void DeserializeTrainConfig(BinaryReader& r);
	pybind11::dict BuildHyperparams();

	void ReloadTrainingScene();
	void RequestSceneReload();

private:
	HeadlessMonitor() = default;
	~HeadlessMonitor();   

	double liveViewCameraLastTime = 0.0;

	std::mutex metricsMutex;
	std::map<std::string, MetricBuffer> metricSeries;
	uint64_t metricStepCounter = 0;

	Console headlessConsole{ "Headless Console" };

	std::string projectDisplayName;

	std::atomic<bool> training{ false };
	std::atomic<bool> pendingEnd{ false };
	std::atomic<bool> stopRequested{ false };
	std::thread trainingThread;

	mutable std::mutex trainStatusMutex;
	std::string trainStatus;
	std::string trainError;

	bool liveTabActiveLastFrame = false;
	void RefreshLiveViewScene();

	void Begin();
	void End();
	void DrawTrainingMonitorTab();
	void DrawLiveTrainingViewTab();
};