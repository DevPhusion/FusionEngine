#pragma once
#include <string>
#include <vector>
#include <optional>

struct FileDialogFilter {
	std::string name;
	std::string pattern;
};

struct FileDialogOptions {
	std::string title;
	std::vector<FileDialogFilter> filters;   
	std::string defaultExtension;            
	std::string defaultFileName;             
	std::string defaultPath;             

	static FileDialogOptions ForExtension(const std::string& friendlyName,
		const std::string& extension, const std::string& title = "") {
		FileDialogOptions opts;
		opts.title = title;
		opts.defaultExtension = extension;
		opts.filters.push_back({ friendlyName, "*." + extension });
		opts.filters.push_back({ "All Files", "*.*" });
		return opts;
	}
};

class FileDialog {
public:
	static std::optional<std::string> ShowSaveDialog(const FileDialogOptions& options);

	static std::optional<std::string> ShowOpenDialog(const FileDialogOptions& options);

	static std::vector<std::string> ShowOpenDialogMulti(const FileDialogOptions& options);

	static std::optional<std::string> ShowFolderDialog(const std::string& title = "");

private:
	FileDialog() = delete; 

	static std::string WideToUtf8(const std::wstring& wide);
	static std::wstring Utf8ToWide(const std::string& utf8);
	static std::optional<std::string> GetResultPath(struct IShellItem* item);
	static void ApplyOptions(struct IFileDialog* dialog, const FileDialogOptions& options);
};