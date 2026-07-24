#pragma once
#include <string>
#include <iostream>
#include "../../../imgui/imgui.h"
#include "../../../imgui/imgui_impl_glfw.h"
#include "../../../imgui/imgui_impl_opengl3.h"

class EditorWindow
{
public:
	EditorWindow(std::string name);
	EditorWindow() = default;

	std::string name;
	bool hidden = false;

	virtual void Hide();
	virtual void Show();
 	virtual void ProcessWindow() = 0;
};

