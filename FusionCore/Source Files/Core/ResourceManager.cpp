#include "../../Header Files/Core/ResourceManager.h"
#include "../../Header Files/Resources/Image.h"
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

ResourceManager::ResourceManager() {
	RegisterType<Image>(ResourceIconType::Image);
}

bool ResourceManager::IsResourceType(ResourceIconType type) {
	return type != ResourceIconType::Folder
		&& type != ResourceIconType::Script
		&& type != ResourceIconType::Scene;
}

Resource* ResourceManager::GetResource(const std::string& virtualPath) {
	auto it = resources.find(virtualPath);
	if (it != resources.end())
		return it->second.get();

	FileManager& fm = FileManager::getInstance();
	fs::path abs = fm.VirtualToAbsolute(virtualPath);

	std::error_code ec;
	if (!fs::is_regular_file(abs, ec) || ec)
		return nullptr;

	std::string ext = abs.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

	const ResourceIconType type = FileManager::ClassifyExtension(ext);
	if (!IsResourceType(type))
		return nullptr;

	std::unique_ptr<Resource> res;
	auto creator = creators.find(type);
	if (creator != creators.end())
		res = creator->second(virtualPath, abs);
	else
		res = std::make_unique<Resource>(virtualPath, abs);

	res->Reload();

	Resource* raw = res.get();
	resources.emplace(virtualPath, std::move(res));
	return raw;
}

void ResourceManager::Invalidate(const std::string& virtualPath) {
	const std::string prefix = virtualPath + "/";

	for (auto it = resources.begin(); it != resources.end();) {
		const std::string& key = it->first;
		if (key == virtualPath || key.rfind(prefix, 0) == 0) {
			it->second->Unload();
			it = resources.erase(it);
		}
		else {
			++it;
		}
	}
}

void ResourceManager::Clear() {
	for (auto& [path, res] : resources)
		res->Unload();
	resources.clear();
}