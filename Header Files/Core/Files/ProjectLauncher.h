#pragma once
#include <string>
#include <vector>
#include "../EngineManager.h"
#include "FileManager.h"
#include "FileDialog.h"
#include "../../../imgui/imgui.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cctype>

struct ProjectEntry {
	std::string name;               
	std::string folderPath;         
	std::string fusionFilePath;     
	std::string lastModifiedText;   
	long long lastModifiedTime = 0; 
	bool missing = false;           
};

class ProjectLauncher {
public:
	static ProjectLauncher& getInstance() {
		static ProjectLauncher instance;
		return instance;
	}

	void Setup(GLFWwindow* window);

	void ProcessLauncher();

	bool OpenProjectFile(const std::string& fusionFilePath);

	bool HasEnteredProject() const { return enteredProject; }

private:
	ProjectLauncher() = default;

	GLFWwindow* window = nullptr;
	std::vector<ProjectEntry> projects;
	int selectedIndex = -1;
	bool enteredProject = false;
	char filterBuf[128] = {};
	std::string errorMessage;

	char newProjectNameBuf[128] = {};
	std::string newProjectFolder;

	std::string ConfigFilePath() const;
	void LoadProjectList();
	void SaveProjectList();

	bool RefreshEntry(ProjectEntry& entry);

	void AddProjectFolder(const std::string& folderPath, const std::string& displayName);

	void ImportProject();
	void CreateProjectFromPopup();
	void ProcessNewProjectPopup();
	void RemoveProject(int index);
	void SortProjects();
};