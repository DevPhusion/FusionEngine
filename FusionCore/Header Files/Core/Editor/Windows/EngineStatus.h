#pragma once
#include "../EditorWindow.h"
#include "../../EngineManager.h"
#include "../../Files/ProjectExportManager.h"

class EngineStatus : public EditorWindow
{
public:
	EngineStatus(std::string name);
	
	GLFWwindow* Window = nullptr; 

	std::string InteractModeText;

	virtual void ProcessWindow();

	void DrawGizmoModeSelector();
	void ProcessSettingsPopup();
	void OnInteractModeChanged();
	void ProcessUnsavedChangesPopup();
	void ProcessExportPopup();
	void ProcessExportingPopup();

	void OnKeyButtonPressed(int key, int scancode, int action, int mods);
private:
	char exportNameBuf[128] = "";
	char exportVersionBuf[32] = "1.0";
	char exportIconBuf[256] = "Resources/Images/engineIcon.png";
	char exportAuthorBuf[128] = "Unknown";
	bool exportAutoZip = true;
	std::string exportFolder;
	std::string exportErrorMessage;
};

