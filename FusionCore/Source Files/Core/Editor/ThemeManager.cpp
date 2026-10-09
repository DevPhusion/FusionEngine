#include "../../../Header Files/Core/Editor/ThemeManager.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fs = std::filesystem;
using json = nlohmann::json;
using Theme = EditorTheme::Theme;

namespace {
	const fs::path kThemeDir = "Resources/Themes";
	const fs::path kSettingsFile = "Resources/editor_settings.json";
	constexpr const char* kDefaultName = "Default";
	constexpr int kFormatVersion = 1;

	void SetErr(std::string* err, const std::string& msg) { if (err) *err = msg; }

	std::string ToLower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	fs::path ThemePath(const std::string& name) { return kThemeDir / (name + ".json"); }

	std::string ToHex(const ImVec4& c) {
		auto b = [](float v) { return (int)(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
		char buf[16];
		if (b(c.w) == 255) std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", b(c.x), b(c.y), b(c.z));
		else std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", b(c.x), b(c.y), b(c.z), b(c.w));
		return buf;
	}

	bool FromHex(const std::string& s, ImVec4& out) {
		size_t i = (!s.empty() && s[0] == '#') ? 1 : 0;
		size_t n = s.size() - i;
		if (n != 6 && n != 8) return false;

		unsigned v[4] = { 0, 0, 0, 255 };
		for (size_t k = 0; k < n / 2; k++) {
			char a = s[i + k * 2], b = s[i + k * 2 + 1];
			if (!std::isxdigit((unsigned char)a) || !std::isxdigit((unsigned char)b)) return false;
			v[k] = (unsigned)std::stoul(s.substr(i + k * 2, 2), nullptr, 16);
		}
		out = ImVec4(v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f);
		return true;
	}

	json ThemeToJson(const std::string& name, const Theme& t) {
		json colors = json::object();
		for (const auto& e : ThemeManager::Entries())
			colors[e.key] = ToHex(t.*(e.member));
		return json{ {"name", name}, {"version", kFormatVersion}, {"colors", colors} };
	}

	bool ParseTheme(const json& j, Theme& out, std::string* err) {
		if (!j.is_object() || !j.contains("colors") || !j["colors"].is_object()) {
			SetErr(err, "File is not a theme (missing \"colors\" object).");
			return false;
		}
		const json& colors = j["colors"];
		Theme t;
		int found = 0;
		for (const auto& e : ThemeManager::Entries()) {
			auto it = colors.find(e.key);
			if (it == colors.end()) continue;
			ImVec4 col;
			if (!it->is_string() || !FromHex(it->get<std::string>(), col)) {
				SetErr(err, std::string("Invalid color value for \"") + e.key + "\" (expected #RRGGBB or #RRGGBBAA).");
				return false;
			}
			t.*(e.member) = col;
			found++;
		}
		if (found == 0) {
			SetErr(err, "Theme contains no recognised colors.");
			return false;
		}
		out = t;
		return true;
	}

	bool ReadJsonFile(const fs::path& path, json& out, std::string* err) {
		std::ifstream f(path);
		if (!f) { SetErr(err, "Could not open " + path.string()); return false; }
		out = json::parse(f, nullptr, false);
		if (out.is_discarded()) { SetErr(err, "File is not valid JSON."); return false; }
		return true;
	}

	bool WriteJsonFile(const fs::path& path, const json& j, std::string* err) {
		std::ofstream f(path);
		if (!f) { SetErr(err, "Could not write " + path.string()); return false; }
		f << j.dump(4);
		return true;
	}
}

ThemeManager& ThemeManager::getInstance() {
	static ThemeManager instance;
	return instance;
}

const std::vector<ThemeManager::ColorEntry>& ThemeManager::Entries() {
	static const std::vector<ColorEntry> entries = {
		{ "accent",       "Accent",          "Accent",   &Theme::accent },
		{ "accentDim",    "Accent (dim)",    "Accent",   &Theme::accentDim },
		{ "danger",       "Danger",          "Accent",   &Theme::danger },

		{ "bg0",          "Title / tab bar", "Surfaces", &Theme::bg0 },
		{ "bg1",          "Window",          "Surfaces", &Theme::bg1 },
		{ "bg2",          "Panel / popup",   "Surfaces", &Theme::bg2 },
		{ "fill0",        "Input",           "Surfaces", &Theme::fill0 },
		{ "fill1",        "Button",          "Surfaces", &Theme::fill1 },
		{ "fill2",        "Button hover",    "Surfaces", &Theme::fill2 },
		{ "border",       "Border",          "Surfaces", &Theme::border },

		{ "text",         "Text",            "Text",     &Theme::text },
		{ "textDisabled", "Disabled text",   "Text",     &Theme::textDisabled },

		{ "axisX",        "X axis",          "Axes",     &Theme::axisX },
		{ "axisY",        "Y axis",          "Axes",     &Theme::axisY },
		{ "axisZ",        "Z axis",          "Axes",     &Theme::axisZ },
	};
	return entries;
}

bool ThemeManager::IsDefault(const std::string& name) {
	return ToLower(name) == ToLower(kDefaultName);
}

std::string ThemeManager::SanitizeName(const std::string& raw) {
	std::string out;
	for (char c : raw) {
		if (std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') out += c;
	}
	size_t start = out.find_first_not_of(' ');
	size_t end = out.find_last_not_of(' ');
	if (start == std::string::npos) return "";
	return out.substr(start, end - start + 1);
}

bool ThemeManager::ThemeExists(const std::string& name) const {
	const std::string lower = ToLower(name);
	for (const auto& n : names)
		if (ToLower(n) == lower) return true;
	return false;
}

void ThemeManager::Initialize() {
	std::error_code ec;
	fs::create_directories(kThemeDir, ec);

	{
		json j;
		Theme dummy;
		const bool ok = fs::exists(ThemePath(kDefaultName), ec)
			&& ReadJsonFile(ThemePath(kDefaultName), j, nullptr)
			&& ParseTheme(j, dummy, nullptr);
		if (!ok) WriteJsonFile(ThemePath(kDefaultName), ThemeToJson(kDefaultName, Theme{}), nullptr);
	}

	Refresh();

	std::string wanted = kDefaultName;
	json settings;
	if (fs::exists(kSettingsFile, ec) && ReadJsonFile(kSettingsFile, settings, nullptr)
		&& settings.is_object() && settings.contains("theme") && settings["theme"].is_string()) {
		wanted = settings["theme"].get<std::string>();
	}

	Theme t;
	if (!LoadTheme(wanted, t, nullptr)) {
		wanted = kDefaultName;
		if (!LoadTheme(wanted, t, nullptr)) t = Theme{};
	}
	t.name = wanted;
	activeName = wanted;
	EditorTheme::currentTheme = t;
}

void ThemeManager::Refresh() {
	names.clear();
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(kThemeDir, ec)) {
		if (ec) break;
		if (!entry.is_regular_file(ec) || entry.path().extension() != ".json") continue;

		json j;
		Theme dummy;
		if (ReadJsonFile(entry.path(), j, nullptr) && ParseTheme(j, dummy, nullptr))
			names.push_back(entry.path().stem().string());
	}
	std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
		return ToLower(a) < ToLower(b);
		});
}

bool ThemeManager::LoadTheme(const std::string& name, Theme& out, std::string* err) const {
	json j;
	if (!ReadJsonFile(ThemePath(name), j, err)) return false;
	if (!ParseTheme(j, out, err)) return false;
	out.name = name;
	return true;
}

void ThemeManager::PersistActive() const {
	WriteJsonFile(kSettingsFile, json{ {"theme", activeName} }, nullptr);
}

bool ThemeManager::SelectTheme(const std::string& name, Theme& out, std::string* err) {
	if (!LoadTheme(name, out, err)) return false;
	activeName = name;
	PersistActive();
	return true;
}

bool ThemeManager::SaveTheme(const std::string& rawName, const Theme& theme, std::string* err) {
	std::string name = SanitizeName(rawName);
	if (name.empty()) { SetErr(err, "Enter a theme name (letters, numbers, spaces, - and _)."); return false; }

	std::error_code ec;
	fs::create_directories(kThemeDir, ec);
	if (!WriteJsonFile(ThemePath(name), ThemeToJson(name, theme), err)) return false;

	Refresh();
	activeName = name;
	PersistActive();
	return true;
}

bool ThemeManager::CreateTheme(const std::string& rawName, const Theme& theme, std::string* err) {
	std::string name = SanitizeName(rawName);
	if (name.empty()) { SetErr(err, "Enter a theme name (letters, numbers, spaces, - and _)."); return false; }
	if (ThemeExists(name)) { SetErr(err, "A theme named \"" + name + "\" already exists."); return false; }
	return SaveTheme(name, theme, err);
}

bool ThemeManager::RenameTheme(const std::string& oldName, const std::string& rawNew, std::string* err) {
	std::string newName = SanitizeName(rawNew);
	if (newName.empty()) { SetErr(err, "Enter a theme name (letters, numbers, spaces, - and _)."); return false; }
	if (IsDefault(oldName)) { SetErr(err, "The Default theme can't be renamed."); return false; }
	if (IsDefault(newName)) { SetErr(err, "\"Default\" is reserved. Pick another name."); return false; }

	const bool onlyCaseChange = ToLower(oldName) == ToLower(newName);
	if (!onlyCaseChange && ThemeExists(newName)) {
		SetErr(err, "A theme named \"" + newName + "\" already exists.");
		return false;
	}

	std::error_code ec;
	fs::rename(ThemePath(oldName), ThemePath(newName), ec);
	if (ec) { SetErr(err, "Rename failed: " + ec.message()); return false; }

	Refresh();
	if (activeName == oldName) { activeName = newName; PersistActive(); }
	return true;
}

bool ThemeManager::ResetDefault(Theme& out, std::string* err) {
	std::error_code ec;
	fs::create_directories(kThemeDir, ec);
	if (!WriteJsonFile(ThemePath(kDefaultName), ThemeToJson(kDefaultName, Theme{}), err)) return false;
	Refresh();
	out = Theme{};
	out.name = kDefaultName;
	return true;
}

bool ThemeManager::ImportTheme(const std::string& path, std::string* outName, std::string* err) {
	json j;
	if (!ReadJsonFile(path, j, err)) return false;

	Theme t;
	if (!ParseTheme(j, t, err)) return false;

	std::string base = SanitizeName(fs::path(path).stem().string());
	if (base.empty()) base = "Imported Theme";

	std::string name = base;
	for (int n = 1; ThemeExists(name); n++)
		name = base + " (" + std::to_string(n) + ")";

	std::error_code ec;
	fs::create_directories(kThemeDir, ec);
	if (!WriteJsonFile(ThemePath(name), ThemeToJson(name, t), err)) return false;

	Refresh();
	if (outName) *outName = name;
	return true;
}

bool ThemeManager::ExportTheme(const std::string& name, const std::string& folder, std::string* err) {
	std::error_code ec;
	fs::copy_file(ThemePath(name), fs::path(folder) / (name + ".json"),
		fs::copy_options::overwrite_existing, ec);
	if (ec) { SetErr(err, "Export failed: " + ec.message()); return false; }
	return true;
}

bool ThemeManager::DeleteTheme(const std::string& name, std::string* err) {
	if (IsDefault(name)) { SetErr(err, "The Default theme can't be deleted (you can reset it instead)."); return false; }
	std::error_code ec;
	fs::remove(ThemePath(name), ec);
	if (ec) { SetErr(err, "Delete failed: " + ec.message()); return false; }
	Refresh();
	return true;
}