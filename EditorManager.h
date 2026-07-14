#pragma once
#include "Inspector.h"
#include "EngineStatus.h"
#include "Hierarchy.h"
#include <vector>
class EditorManager
{
public:
	EditorManager(const EditorManager&) = delete;
	void operator=(const EditorManager&) = delete;

	static EditorManager& getInstance() {
		static EditorManager instance;
		return instance;
	}

	Object* selectedObject;

	std::vector<EditorWindow*> Windows;
	bool WindowHovered; // mouse action in windows
	bool WindowTyped; //keyboard action in windows

	void Setup(GLFWwindow* window);
	void AddWindow(EditorWindow* window);
	void SetSelectedObject(Object* object);
	void ProcessEditor();
	void ProcessDockSpace();

private:
	EditorManager() = default;
};

