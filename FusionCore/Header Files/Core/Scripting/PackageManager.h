#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <atomic>

struct Package {
	std::string id;                            
	std::string displayName;                   
	std::string description;
	std::vector<std::string> pipRequirements; 
};

struct PackageEntry {
	std::string id;
	std::vector<std::string> requirements;
	bool selected = true;  
	bool confirmedInstalled = false;
};

enum class PackageSyncStatus {
	Idle,
	Syncing,
	Done,
	Failed
};

class PackageManager {
public:
	static PackageManager& getInstance() {
		static PackageManager instance;
		return instance;
	}

	PackageManager(const PackageManager&) = delete;
	PackageManager& operator=(const PackageManager&) = delete;

	const std::vector<Package>& GetAvailablePackages() const;
	const Package* FindDefinition(const std::string& id) const;

	void LoadForProject(const std::string& projectDirectory);
	void SaveManifest();

	bool IsPackageSelected(const std::string& id) const;
	bool IsPackageInstalled(const std::string& id) const;
	std::vector<std::string> GetInstalledPackageIds() const;
	void SelectPackage(const std::string& id);     
	void DeselectPackage(const std::string& id);    

	bool NeedsSync() const;
	bool NeedsBindingsRebuild() const;
	void MarkBindingsUpToDate();


	bool SyncInstalledPackages(const std::filesystem::path& venvPython);

	PackageSyncStatus GetSyncStatus() const { return syncStatus.load(); }
	std::string GetSyncMessage() const;

private:
	PackageManager() = default;

	std::filesystem::path ManifestPath() const;
	void SetSyncStatus(PackageSyncStatus status, const std::string& message);

	std::string projectDirectory;
	std::vector<PackageEntry> entries;

	std::atomic<PackageSyncStatus> syncStatus{ PackageSyncStatus::Idle };
	mutable std::mutex statusMutex;
	std::string syncMessage;

	std::vector<std::string> boundPackageIds; 
};