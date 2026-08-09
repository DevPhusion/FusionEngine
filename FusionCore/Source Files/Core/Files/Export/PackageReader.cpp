#include "../../../../Header Files/Core/Files/Export/PackageReader.h"
#include "../../../../Header Files/Core/Files/Export/PackageCrypto.h"
#include <fstream>

namespace {
	std::string ModuleNameToVirtualPath(const std::string& moduleDotted) {
		std::string path = moduleDotted;
		std::replace(path.begin(), path.end(), '.', '/');
		return "res://" + path + ".py";
	}
}

bool PackageReader::Load(const std::filesystem::path& packPath) {
	std::ifstream in(packPath, std::ios::binary);
	if (!in.is_open()) return false;

	uint32_t magic = 0, version = 0, entryCount = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	in.read(reinterpret_cast<char*>(&version), sizeof(version));
	in.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
	if (magic != 0x4B415046) return false;

	PackageCrypto::Salt salt{};
	in.read(reinterpret_cast<char*>(salt.data()), salt.size());
	std::array<uint8_t, 16> key = PackageCrypto::DeriveKey(salt);

	struct RawEntry { std::string path; uint64_t offset, size; };
	std::vector<RawEntry> raw(entryCount);

	for (auto& e : raw) {
		uint32_t pathLen = 0;
		in.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
		e.path.resize(pathLen);
		in.read(e.path.data(), pathLen);
		in.read(reinterpret_cast<char*>(&e.offset), sizeof(uint64_t));
		in.read(reinterpret_cast<char*>(&e.size), sizeof(uint64_t));
	}

	std::vector<uint8_t> payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

	entries.clear();
	for (auto& e : raw) {
		std::vector<uint8_t> chunk(payload.begin() + e.offset, payload.begin() + e.offset + e.size);
		PackageCrypto::XorBuffer(chunk, key);
		entries[e.path] = std::move(chunk);
	}
	return true;
}

const std::vector<uint8_t>* PackageReader::Get(const std::string& virtualPath) const {
	auto it = entries.find(virtualPath);
	return it != entries.end() ? &it->second : nullptr;
}


bool PackageReader::HasModule(const std::string& moduleDotted) const {
	return entries.count(ModuleNameToVirtualPath(moduleDotted)) != 0;
}

bool PackageReader::IsPackage(const std::string& moduleDotted) const {
	std::string path = moduleDotted;
	std::replace(path.begin(), path.end(), '.', '/');
	std::string prefix = "res://" + path + "/";

	for (auto& [key, value] : entries) {
		if (key.rfind(prefix, 0) == 0) return true;
	}
	return false;
}

std::string PackageReader::GetSource(const std::string& moduleDotted) const {
	auto it = entries.find(ModuleNameToVirtualPath(moduleDotted));
	if (it == entries.end()) return "";
	return std::string(reinterpret_cast<const char*>(it->second.data()), it->second.size());
}