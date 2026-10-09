#pragma once
#include "../EditorWindow.h"
#include "../ThemeManager.h"
#include <string>

class SettingsWindow : public EditorWindow {
public:
	SettingsWindow(std::string name);

	static void Open() { openRequested = true; }
	void ProcessWindow() override;

private:
	static inline bool openRequested = false;

	bool isOpen = false;
	int category = 0;

	ThemeManager::Theme editTheme;
	char newThemeNameBuf[64] = "";
	char themeNameBuf[64] = "";
	bool themeDirty = false;
	bool pendingApply = false;
	bool editorSynced = false;
	std::string themeStatus;
	bool themeStatusIsError = false;

	void DrawGeneral();
	void DrawPhysics();
	void DrawViewport();
	void DrawDebugDraw();
	void DrawThemes();

	void LoadThemeIntoEditor(const std::string& name);
	void SetThemeStatus(const std::string& msg, bool isError);
};