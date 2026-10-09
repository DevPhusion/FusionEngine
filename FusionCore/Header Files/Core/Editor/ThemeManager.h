#pragma once
#include "EditorTheme.h"
#include <string>
#include <vector>

class ThemeManager {
public:
	using Theme = EditorTheme::Theme;

	struct ColorEntry {
		const char* key;
		const char* label;
		const char* group;
		ImVec4 Theme::* member;
	};

	static ThemeManager& getInstance();
	static const std::vector<ColorEntry>& Entries();
	static bool IsDefault(const std::string& name);  
	static std::string SanitizeName(const std::string& raw);

	void Initialize();

	const std::vector<std::string>& GetThemeNames() const { return names; }
	const std::string& GetActiveName() const { return activeName; }
	bool ThemeExists(const std::string& name) const;

	bool SelectTheme(const std::string& name, Theme& out, std::string* err = nullptr);

	bool SaveTheme(const std::string& name, const Theme& theme, std::string* err = nullptr);

	bool CreateTheme(const std::string& name, const Theme& theme, std::string* err = nullptr);

	bool RenameTheme(const std::string& oldName, const std::string& newName, std::string* err = nullptr);

	bool ResetDefault(Theme& out, std::string* err = nullptr);

	bool ImportTheme(const std::string& path, std::string* outName, std::string* err = nullptr);
	bool ExportTheme(const std::string& name, const std::string& folder, std::string* err = nullptr);
	bool DeleteTheme(const std::string& name, std::string* err = nullptr);

private:
	ThemeManager() = default;

	void Refresh();
	bool LoadTheme(const std::string& name, Theme& out, std::string* err) const;
	void PersistActive() const;

	std::vector<std::string> names;
	std::string activeName = "Default";
};