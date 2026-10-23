#pragma once
#include "../Resources/Resource.h"
#include "Files/FileManager.h"   
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
class ResourceManager
{
public:
	static ResourceManager& getInstance() {
		static ResourceManager instance;
		return instance;
	}

	ResourceManager(const ResourceManager&) = delete;
	void operator=(const ResourceManager&) = delete;

	using Creator = std::function<std::unique_ptr<Resource>(std::string virtualPath, std::filesystem::path absolutePath)>;
	
	template<typename T>
	void RegisterType(ResourceIconType type) {
		creators[type] = [](std::string v, std::filesystem::path a) {
			return std::make_unique<T>(std::move(v), std::move(a));
			};
	}

	static bool IsResourceType(ResourceIconType type);

	Resource* GetResource(const std::string& virtualPath);

	void Invalidate(const std::string& virtualPath);

	void Clear();

private:
	ResourceManager();

	std::unordered_map<ResourceIconType, Creator> creators;
	std::unordered_map<std::string, std::unique_ptr<Resource>> resources;
};