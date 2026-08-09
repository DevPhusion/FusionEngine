#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

class PackageReader {
public:
	static PackageReader& getInstance() {
		static PackageReader instance;
		return instance;
	}
	PackageReader(const PackageReader&) = delete;
	void operator=(const PackageReader&) = delete;

	bool Load(const std::filesystem::path& packPath);
	const std::vector<uint8_t>* Get(const std::string& virtualPath) const;

	bool HasModule(const std::string& moduleDotted) const;
	bool IsPackage(const std::string& moduleDotted) const;
	std::string GetSource(const std::string& moduleDotted) const;

private:
	PackageReader() = default;
	std::unordered_map<std::string, std::vector<uint8_t>> entries;
};