#pragma once
#include "../EditorWindow.h"
#include "../../EngineManager.h"
#include "../../Files/Export/ProjectExportManager.h"

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
	std::string exportErrorMessage;
};

