#pragma once
#include "../EditorWindow.h"
#include "../../SceneManager.h"
#include <string>

class SceneTab : public EditorWindow
{
public:
	SceneTab(std::string name);

	virtual void ProcessWindow();

	static bool SaveActiveScene();

private:
	void ProcessTabBar();
	void ProcessCloseConfirmPopup();
	void RequestClose(int index);

	int pendingCloseIndex = -1;
};