#pragma once
#include "../EditorWindow.h"
#include "../../../Objects/Object.h"
#include <vector>
#include <string>
#include <cfloat>
#include <cctype>
#include <algorithm>

class AddObjectWindow : public EditorWindow
{
public:
	std::vector<std::string> ObjectTypes = { "Object", "Camera", "Rigid Box", "Rigid Circle", "Rigid Polygon", "Soft Box", "Soft Circle", "Soft Polygon", "Fluid", "Gas" };
	std::string SelectedType = "";
	std::string SelectedScenePath = "";
	Object* parent = nullptr;

	std::vector<std::string> sceneFiles;
	void RefreshSceneList();

	AddObjectWindow(std::string name);
	virtual void Show();
	virtual void ProcessWindow();

private:
	char filter[64] = "";
	bool justOpened = false;

	char nameBuf[64] = "";
	void ApplyName(Object* obj);

	void DrawBrowser();
	void DrawVertexMode();
	void CommitAdd();
	void Cancel();
};