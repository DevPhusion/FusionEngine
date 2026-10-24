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

	using ResourceCreator = std::function<std::unique_ptr<Resource>(std::string virtualPath, std::filesystem::path absolutePath)>;
	
	template<typename T>
	void RegisterType(ResourceType type) {
		creators[type] = [](std::string v, std::filesystem::path a) {
			return std::make_unique<T>(std::move(v), std::move(a));
			};
	}

	static bool IsResourceType(ResourceType type);

	Resource* GetResource(const std::string& virtualPath);

	void Invalidate(const std::string& virtualPath);

	void Clear();

private:
	ResourceManager();

	std::unordered_map<ResourceType, ResourceCreator> creators;
	std::unordered_map<std::string, std::unique_ptr<Resource>> resources;
};