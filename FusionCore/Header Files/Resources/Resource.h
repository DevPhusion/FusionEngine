#pragma once
#include <string>
#include <filesystem>
#include <cstdint>

class Resource
{
public:
	Resource(std::string virtualPath, std::filesystem::path absolutePath);
	virtual ~Resource() = default;

	Resource(const Resource&) = delete;
	Resource& operator=(const Resource&) = delete;

	const std::string& GetVirtualPath() const { return virtualPath; }
	const std::filesystem::path& GetAbsolutePath() const { return absolutePath; }
	uintmax_t GetFileSize() const { return fileSize; }
	bool Exists() const { return exists; }

	virtual std::string GetTypeName() const;

	void Reload();
	void DrawPreviewWidget(float maxHeight = -1.0f);

	bool CheckForChanges();

	virtual void Unload() {}

	virtual void ProcessInspectorUI();

protected:
	float previewMaxHeight = -1.0f;

	virtual void OnReload() {}
	virtual void DrawPreview() {}
	virtual void DrawProperties() {}
	static std::string FormatBytes(uintmax_t bytes);

private:
	void DrawHeader();
	void DrawCommonProperties();

	std::string virtualPath;
	std::filesystem::path absolutePath;

	uintmax_t fileSize = 0;
	std::filesystem::file_time_type modified{};
	std::string modifiedText;
	bool exists = false;
};