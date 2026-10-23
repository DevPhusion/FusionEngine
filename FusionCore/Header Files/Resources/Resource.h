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

	// Display name for the "Type" row, e.g. "PNG image". Base returns "<EXT> file".
	virtual std::string GetTypeName() const;

	// (Re)reads file info from disk, then calls OnReload() so subclasses can refresh their own data.
	void Reload();

	// Cheap stat() check; reloads if the file changed or disappeared. Returns true if it reloaded.
	bool CheckForChanges();

	// Release anything the resource owns (GPU textures etc). Called before removal/rename/project switch.
	virtual void Unload() {}

	// Draws the whole Inspector panel for this resource. Override to take full control, or
	// (usually) just override DrawPreview / DrawProperties below.
	virtual void ProcessInspectorUI();

protected:
	// Subclass hook: load type-specific metadata.
	virtual void OnReload() {}

	// Subclass hook: large preview shown under the file name (image, waveform, ...).
	virtual void DrawPreview() {}

	// Subclass hook: extra rows appended after the common ones. Call PropertyRow() inside.
	virtual void DrawProperties() {}

	static void PropertyRow(const char* label, const std::string& value, bool wrap = false);
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