#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

#include <algorithm>
#include <fstream>
#include <cctype>
#include "../../../stb_image.h"

enum class ResourceIconType {
	Folder,
	Image,
	Unknown
};

struct FileSystemEntry {
	std::string name;
	std::filesystem::path absolutePath;
	std::string virtualPath;
	bool isDirectory = false;
	ResourceIconType iconType = ResourceIconType::Unknown;
	unsigned int thumbnailTexture = 0;
};

class FileManager
{
public:
	static FileManager& getInstance() {
		static FileManager instance;
		return instance;
	}

	FileManager(const FileManager&) = delete;
	void operator=(const FileManager&) = delete;

	static constexpr const char* kResourceDragDropPayloadType = "RESOURCE_VPATH";

	bool isSaved = false;
	std::string currentProjectFile = "";
	std::string currentProjectDirectory = "";

	void SetupResourcesFolder();

	const std::filesystem::path& GetResourcesRoot() const { return resourcesRoot; }
	std::string GetRootVirtualPath() const { return "res://"; }
	int GetResourceGeneration() const { return resourceGeneration; }

	std::vector<FileSystemEntry> GetDirectoryContents(const std::string& virtualPath) const;
	bool IsDirectory(const std::string& virtualPath) const;
	bool VirtualPathExists(const std::string& virtualPath) const;

	std::filesystem::path VirtualToAbsolute(const std::string& virtualPath) const;
	std::string AbsoluteToVirtual(const std::filesystem::path& absolutePath) const;

	bool CreateFolder(const std::string& parentVirtualPath, const std::string& folderName);
	bool DeleteResource(const std::string& virtualPath);
	bool ImportFile(const std::string& sourceAbsolutePath, const std::string& destVirtualDirectory);
	bool RenameResource(const std::string& virtualPath, const std::string& newName);
	bool MoveResource(const std::string& virtualPath, const std::string& destDirVirtualPath);

	unsigned int GetOrLoadThumbnail(const std::filesystem::path& imageAbsolutePath);
	void ClearThumbnailCache();

	void SaveProjectToFile(const std::string& path);
	void LoadProjectFromFile(const std::string& path);
	void NewProject();

private:
	FileManager() = default;
	~FileManager();

	static ResourceIconType ClassifyExtension(const std::string& extensionLower);

	static constexpr uint32_t magicByte = 0x4E535546;
	static constexpr uint32_t version = 1;

	std::filesystem::path resourcesRoot;
	std::unordered_map<std::string, unsigned int> thumbnailCache;
	int resourceGeneration = 0;
};