#pragma once
#include "EditorWindow.h"
#include "EngineManager.h"
class EngineStatus : public EditorWindow
{
public:
	EngineStatus(std::string name);
	
	std::string InteractModeText;

	virtual void ProcessWindow();

	void DrawGizmoModeSelector();
	void ProcessSettingsPopup();
	void OnInteractModeChanged();
};

