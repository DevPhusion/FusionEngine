#pragma once
#include "../EditorWindow.h"
#include "../../Files/FileManager.h"
#include <string>
#include <vector>
#include <set>

class FileSystem : public EditorWindow
{
public:
	FileSystem(std::string name);
	FileSystem() = default;

	virtual void ProcessWindow();

private:
	void ProcessToolbar();
	void ProcessFilterBar();
	void ProcessEntries();
	void ProcessCreateFolderPopup();
	void ProcessCreateScriptPopup();
	void ProcessEntryContextMenu(const FileSystemEntry& entry);

	void DrawNode(const FileSystemEntry& entry, int depth);

	void SelectPath(const std::string& virtualPath);
	void NavigateBack();
	void NavigateForward();
	void ExpandParentsOf(const std::string& virtualPath);

	void BeginRename(const std::string& virtualPath, const std::string& currentName);
	void CommitRename();
	void CancelRename();

	void MoveEntry(const std::string& sourceVirtualPath, const std::string& destDirVirtualPath);

	void AddFileToFolder(const std::string& folderVirtualPath);

	std::string GetParentVirtualPath(const std::string& virtualPath) const;
	bool SubtreeMatchesFilter(const std::string& virtualPath, const std::string& filterLower) const;

	std::string selectedPath;
	std::set<std::string> expandedPaths;

	std::vector<std::string> history;
	int historyIndex = -1;
	bool navigatingHistory = false;
	std::string pendingScrollTarget;

	std::string renamingPath;
	char renameBuf[128] = "";
	bool renameJustOpened = false;

	char filterBuf[128] = "";
	char pathDisplayBuf[256] = "";
	char newFolderNameBuf[128] = "";
	std::string newFolderTargetPath;
	bool newFolderPopupRequested = false;

	bool newScriptPopupRequested = false;
	std::string newScriptTargetPath;
	char newScriptNameBuf[128] = "";

	int lastSeenGeneration = -1;
};