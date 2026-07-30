#pragma once
#include "../EditorWindow.h"
#include <vector>

class AddObjectWindow : public EditorWindow
{
public:
	std::vector<std::string> ObjectTypes = {"Object", "Camera", "Rigid Box", "Rigid Circle", "Rigid Polygon", "Soft Box", "Soft Circle", "Soft Polygon", "Fluid"};
	std::string SelectedType = "";

	AddObjectWindow(std::string name);
	virtual void ProcessWindow();
};

