#pragma once
#include "../EditorWindow.h"
#include "../../EngineManager.h"
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

	void OnKeyButtonPressed(int key, int scancode, int action, int mods);
};

