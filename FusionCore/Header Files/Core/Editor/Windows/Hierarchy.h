#pragma once
#include "../EditorWindow.h"
#include "AddObjectWindow.h"
#include "../../InputManager.h"
#include <string>

class Object;

class Hierarchy : public EditorWindow
{
public:
	enum class DropZone { Child, Before, After };

	struct PendingMove {
		Object* dragged = nullptr;
		Object* target = nullptr;
		DropZone zone = DropZone::Child;
	};

	Hierarchy(std::string name);

	AddObjectWindow* addObjectWindow = nullptr;

	bool IsRenaming = false;

	virtual void ProcessWindow();
	void OnKeyPressed(int key, int scancode, int action, int mods);

private:
	PendingMove pendingMove;

	void OpenAddObjectWindow(Object* parent);
	void PlaceRelative(Object* dragged, Object* ref, bool after);
	void ApplyHierarchyMove(const PendingMove& m);
	void DrawObjectNode(Object* currentObj, char* filter_buffer, char* renameBuffer);
};