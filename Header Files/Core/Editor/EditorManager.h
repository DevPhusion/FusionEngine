#pragma once
#include "Windows/Inspector.h"
#include "Windows/EngineStatus.h"
#include "Windows/Hierarchy.h"
#include "Windows/FileSystem.h"
#include "Windows/EngineProfiler.h"
#include "Windows/Console.h"
#include "../../../imgui/implot.h"
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
	bool WindowHovered; 
	bool WindowTyped; 

	void Setup(GLFWwindow* window);
	void AddWindow(EditorWindow* window);
	void SetSelectedObject(Object* object);
	void ProcessEditor();
	void ProcessDockSpace();

private:
	EditorManager() = default;
};

