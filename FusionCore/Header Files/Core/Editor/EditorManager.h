#pragma once
#include "Windows/Inspector.h"
#include "Windows/EngineStatus.h"
#include "Windows/Hierarchy.h"
#include "Windows/FileSystem.h"
#include "Windows/EngineProfiler.h"
#include "Windows/Console.h"
#include "Windows/Viewport.h"
#include "Windows/SceneTab.h"
#include "../EngineManager.h"
#include "../../../imgui/implot.h"
#include <vector>

struct UndoEntry {
	std::vector<uint8_t> before;
	std::vector<uint8_t> after;
	std::vector<uint64_t> removeOnUndo;   
	std::vector<uint64_t> removeOnRedo;
	std::vector<uint8_t> constraintsBefore;
	std::vector<uint8_t> constraintsAfter;
	bool hasConstraintSnapshot = false;
};

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
	Viewport* gameViewport = nullptr;

	std::vector<EditorWindow*> Windows;
	bool WindowHovered;
	bool WindowTyped;

	void Setup(GLFWwindow* window);
	void AddWindow(EditorWindow* window);
	void SetSelectedObject(Object* object);
	void ProcessEditor();
	void ProcessDockSpace();

	void RegisterObjectCreated(Object* obj) {
		if (FileManager::getInstance().IsRestoring()) return;
		UndoEntry entry;
		entry.before = FileManager::getInstance().SnapshotObjects({});
		entry.after = FileManager::getInstance().SnapshotObjects({ obj });
		entry.removeOnUndo = { obj->id };   
		undoStack.push_back(std::move(entry));
		if (undoStack.size() > maxUndoDepth) undoStack.pop_front();
		redoStack.clear();
	}

	void RegisterObjectDeleted(std::vector<uint8_t> before, std::vector<uint64_t> removeOnRedoIds, const std::vector<Object*>& survivingChildren) {
		UndoEntry entry;
		entry.before = std::move(before);
		entry.after = FileManager::getInstance().SnapshotObjects(survivingChildren);
		entry.removeOnRedo = std::move(removeOnRedoIds);
		undoStack.push_back(std::move(entry));
		if (undoStack.size() > maxUndoDepth) undoStack.pop_front();
		redoStack.clear();
	}

	void BeginEdit(const std::vector<Object*>& roots, bool includeConstraints = false) {
		if (FileManager::getInstance().IsRestoring()) return;
		if (editDepth == 0) {
			pendingBefore = FileManager::getInstance().SnapshotObjects(roots);
			pendingIncludeConstraints = includeConstraints;
			if (includeConstraints) {
				pendingConstraintsBefore = FileManager::getInstance().SnapshotConstraints();
			}
		}
		else if (includeConstraints && !pendingIncludeConstraints) {
			pendingIncludeConstraints = true;
			pendingConstraintsBefore = FileManager::getInstance().SnapshotConstraints();
		}
		editDepth++;
	}

	void EndEdit(const std::vector<Object*>& roots) {
		if (FileManager::getInstance().IsRestoring()) return;
		if (editDepth == 0) return;
		editDepth--;
		if (editDepth > 0) return;

		auto after = FileManager::getInstance().SnapshotObjects(roots);
		std::vector<uint8_t> constraintsAfter;
		if (pendingIncludeConstraints) {
			constraintsAfter = FileManager::getInstance().SnapshotConstraints();
		}

		bool objectsChanged = (after != pendingBefore);
		bool constraintsChanged = pendingIncludeConstraints && (constraintsAfter != pendingConstraintsBefore);
		if (!objectsChanged && !constraintsChanged) {
			pendingIncludeConstraints = false;
			return;
		}

		UndoEntry entry;
		entry.before = std::move(pendingBefore);
		entry.after = std::move(after);
		entry.hasConstraintSnapshot = pendingIncludeConstraints;
		if (pendingIncludeConstraints) {
			entry.constraintsBefore = std::move(pendingConstraintsBefore);
			entry.constraintsAfter = std::move(constraintsAfter);
		}

		undoStack.push_back(std::move(entry));
		if (undoStack.size() > maxUndoDepth) undoStack.pop_front();
		redoStack.clear();
		pendingIncludeConstraints = false;
	}

	void Undo() {
		if (FileManager::getInstance().IsRestoring()) return;
		if (undoStack.empty()) return;
		auto entry = undoStack.back(); undoStack.pop_back();
		FileManager::getInstance().RestoreObjects(entry.before, entry.removeOnUndo);
		if (entry.hasConstraintSnapshot) FileManager::getInstance().RestoreConstraints(entry.constraintsBefore);
		redoStack.push_back(entry);
	}

	void Redo() {
		if (FileManager::getInstance().IsRestoring()) return;
		if (redoStack.empty()) return;
		auto entry = redoStack.back(); redoStack.pop_back();
		FileManager::getInstance().RestoreObjects(entry.after, entry.removeOnRedo);
		if (entry.hasConstraintSnapshot) FileManager::getInstance().RestoreConstraints(entry.constraintsAfter);
		undoStack.push_back(entry);
	}

private:
	EditorManager() = default;

	int editDepth = 0;
	std::deque<UndoEntry> undoStack;
	std::deque<UndoEntry> redoStack;
	std::vector<uint8_t> pendingBefore;
	std::vector<uint8_t> pendingConstraintsBefore;
	size_t maxUndoDepth = 100;
	bool pendingIncludeConstraints = false;
};
