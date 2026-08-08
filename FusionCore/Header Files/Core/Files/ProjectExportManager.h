#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

struct ExportConfiguration {
	std::string exportFolder;
	std::string name;
	std::string version = "1.0";
	std::string iconPath = "Resources/Images/engineIcon.png";
	std::string author = "Unknown";
	bool autoZipExport = true;
};

class ProjectExportManager
{
public:
	enum class ExportStage {
		Idle,
		CopyingRuntime,
		CopyingProjectData,
		SavingProject,
		Zipping,
		Done,
		Failed
	};

	static ProjectExportManager& getInstance() {
		static ProjectExportManager instance;
		return instance;
	}

	ProjectExportManager(const ProjectExportManager&) = delete;
	void operator=(const ProjectExportManager&) = delete;

	bool StartExport(const ExportConfiguration& config);

	void Update();

	bool IsBusy() const;
	ExportStage GetStage() const { return stage.load(); }
	std::string GetStatusMessage() const;
	const std::string& GetLastError() const { return lastError; }

private:
	ProjectExportManager() = default;

	void RunExport(ExportConfiguration config);
	bool DoExport(const ExportConfiguration& config, std::string& outError);
	void SetStatus(ExportStage newStage, const std::string& message);

	std::atomic<ExportStage> stage{ ExportStage::Idle };
	mutable std::mutex statusMutex;
	std::string statusMessage;

	std::thread workerThread;
	bool backgroundSucceeded = false;
	std::string lastError;
};