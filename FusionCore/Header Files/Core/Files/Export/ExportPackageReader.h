#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

class ExportPackageReader {
public:
	static ExportPackageReader& getInstance() {
		static ExportPackageReader instance;
		return instance;
	}
	ExportPackageReader(const ExportPackageReader&) = delete;
	void operator=(const ExportPackageReader&) = delete;

	bool Load(const std::filesystem::path& packPath);
	const std::vector<uint8_t>* Get(const std::string& virtualPath) const;

	bool HasModule(const std::string& moduleDotted) const;
	bool IsPackage(const std::string& moduleDotted) const;
	std::string GetSource(const std::string& moduleDotted) const;

private:
	ExportPackageReader() = default;
	std::unordered_map<std::string, std::vector<uint8_t>> entries;
};