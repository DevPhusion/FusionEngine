#pragma once
#include "../EditorWindow.h"
#include "AddObjectWindow.h"
#include "../../InputManager.h"
#include <string>

class Object;

class Hierarchy : public EditorWindow
{
public:
	Hierarchy(std::string name);

	AddObjectWindow* addObjectWindow = nullptr;

	bool IsRenaming = false;

	virtual void ProcessWindow();
	void OnKeyPressed(int key, int scancode, int action, int mods);

private:
	void DrawObjectNode(Object* currentObj, char* filter_buffer, char* renameBuffer);
};