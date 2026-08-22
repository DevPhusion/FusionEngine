#include "../../../Header Files/Core/Scripting/PackageManager.h"
#include "../../../Header Files/Core/Scripting/ScriptManager.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace {
	std::string TrimLine(const std::string& s) {
		std::string r = s;
		if (!r.empty() && r.back() == '\r') r.pop_back();
		return r;
	}

	std::string JoinRequirements(const std::vector<std::string>& reqs, char sep) {
		std::string out;
		for (size_t i = 0; i < reqs.size(); i++) {
			if (i) out += sep;
			out += reqs[i];
		}
		return out;
	}

	std::vector<std::string> SplitRequirements(const std::string& joined, char sep) {
		std::vector<std::string> out;
		std::stringstream ss(joined);
		std::string item;
		while (std::getline(ss, item, sep))
			if (!item.empty()) out.push_back(item);
		return out;
	}

	std::string BareName(const std::string& requirement) {
		size_t cut = requirement.find_first_of("<>=! ");
		return cut == std::string::npos ? requirement : requirement.substr(0, cut);
	}
}

const std::vector<Package>& PackageManager::GetAvailablePackages() const {
	static const std::vector<Package> kCatalog = {
		{
			"rl",
			"Reinforcement Learning",
			"Adds tooling for training agents against your scenes: gymnasium environments, "
			"stable-baselines3 algorithms, and PyTorch as the backing tensor library.",
			{ "gymnasium", "stable-baselines3", "torch" }
		},
	};
	return kCatalog;
}

const Package* PackageManager::FindDefinition(const std::string& id) const {
	for (auto& def : GetAvailablePackages())
		if (def.id == id) return &def;
	return nullptr;
}

std::filesystem::path PackageManager::ManifestPath() const {
	return fs::path(projectDirectory) / "fusion_packages.cfg";
}

void PackageManager::LoadForProject(const std::string& newProjectDirectory) {
	projectDirectory = newProjectDirectory;
	entries.clear();

	std::ifstream in(ManifestPath());
	if (!in.is_open()) return;

	std::string idLine, reqLine, selLine, confLine;
	while (std::getline(in, idLine)) {
		if (!std::getline(in, reqLine)) break;
		if (!std::getline(in, selLine)) break;
		if (!std::getline(in, confLine)) break;

		idLine = TrimLine(idLine);
		reqLine = TrimLine(reqLine);
		selLine = TrimLine(selLine);
		confLine = TrimLine(confLine);
		if (idLine.empty()) continue;

		PackageEntry entry;
		entry.id = idLine;
		entry.requirements = SplitRequirements(reqLine, ';');
		entry.selected = (selLine == "1");
		entry.confirmedInstalled = (confLine == "1");
		entries.push_back(std::move(entry));
	}
}

void PackageManager::SaveManifest() {
	if (projectDirectory.empty()) return;

	std::error_code ec;
	fs::create_directories(projectDirectory, ec);

	std::ofstream out(ManifestPath(), std::ios::trunc);
	if (!out.is_open()) return;

	for (auto& e : entries) {
		out << e.id << "\n";
		out << JoinRequirements(e.requirements, ';') << "\n";
		out << (e.selected ? "1" : "0") << "\n";
		out << (e.confirmedInstalled ? "1" : "0") << "\n";
	}
}

bool PackageManager::IsPackageInstalled(const std::string& id) const {
	for (auto& e : entries)
		if (e.id == id) return e.confirmedInstalled;
	return false;
}

bool PackageManager::IsPackageSelected(const std::string& id) const {
	for (auto& e : entries)
		if (e.id == id) return e.selected;
	return false;
}

void PackageManager::SelectPackage(const std::string& id) {
	const Package* def = FindDefinition(id);
	if (!def) return;

	for (auto& e : entries) {
		if (e.id == id) {
			e.selected = true;
			e.requirements = def->pipRequirements;
			SaveManifest();
			return;
		}
	}

	PackageEntry entry;
	entry.id = def->id;
	entry.requirements = def->pipRequirements;
	entry.selected = true;
	entry.confirmedInstalled = false;
	entries.push_back(std::move(entry));
	SaveManifest();
}

void PackageManager::DeselectPackage(const std::string& id) {
	for (auto& e : entries) {
		if (e.id == id) {
			e.selected = false;
			SaveManifest();
			return;
		}
	}
}

bool PackageManager::NeedsSync() const {
	return !entries.empty();
}

std::vector<std::string> PackageManager::GetInstalledPackageIds() const {
	std::vector<std::string> ids;
	for (auto& e : entries)
		if (e.confirmedInstalled) ids.push_back(e.id);
	std::sort(ids.begin(), ids.end());
	return ids;
}

bool PackageManager::NeedsBindingsRebuild() const {
	return GetInstalledPackageIds() != boundPackageIds;
}

void PackageManager::MarkBindingsUpToDate() {
	boundPackageIds = GetInstalledPackageIds();
}

void PackageManager::SetSyncStatus(PackageSyncStatus status, const std::string& message) {
	{
		std::lock_guard<std::mutex> lock(statusMutex);
		syncMessage = message;
	}
	syncStatus.store(status);
}

std::string PackageManager::GetSyncMessage() const {
	std::lock_guard<std::mutex> lock(statusMutex);
	return syncMessage;
}

bool PackageManager::SyncInstalledPackages(const fs::path& venvPython) {
	std::error_code ec;
	if (!fs::exists(venvPython, ec)) {
		SetSyncStatus(PackageSyncStatus::Failed, "Python environment is not ready.");
		return false;
	}

	SetSyncStatus(PackageSyncStatus::Syncing, "Checking packages...");

	bool allOk = true;
	int installedCount = 0;
	int removedCount = 0;
	std::vector<PackageEntry> stillTracked;

	for (auto& entry : entries) {
		if (entry.selected) {
			SetSyncStatus(PackageSyncStatus::Syncing, "Installing " + entry.id + "...");

			std::ostringstream cmd;
			cmd << "\"" << venvPython.string() << "\" -m pip install --upgrade";
			for (auto& req : entry.requirements) cmd << " \"" << req << "\"";

			std::string output;
			int result = ScriptManager::getInstance().RunHiddenCommand(cmd.str(), &output);
			entry.confirmedInstalled = (result == 0);
			if (result != 0) {
				Console::PrintError(
					"PackageManager: failed to install package '{}' (exit code {}): {}"
				).Format(entry.id, result, output);
				allOk = false;
			}
			else {
				installedCount++;
			}
			stillTracked.push_back(entry);
		}
		else {
			SetSyncStatus(PackageSyncStatus::Syncing, "Removing " + entry.id + "...");

			std::ostringstream cmd;
			cmd << "\"" << venvPython.string() << "\" -m pip uninstall -y";
			for (auto& req : entry.requirements) cmd << " \"" << BareName(req) << "\"";

			std::string output;
			int result = ScriptManager::getInstance().RunHiddenCommand(cmd.str(), &output);
			if (result != 0) {
				Console::PrintError(
					"PackageManager: failed to fully uninstall package '{}' (exit code {}): {}"
				).Format(entry.id, result, output);
				allOk = false;
			}
			else {
				removedCount++;
			}
		}
	}

	entries = std::move(stillTracked);
	SaveManifest();

	if (installedCount > 0 || removedCount > 0) {
		Console::Print(
			"PackageManager: sync complete - {} package(s) installed/updated, {} removed."
		).Format(installedCount, removedCount);
	}

	SetSyncStatus(allOk ? PackageSyncStatus::Done : PackageSyncStatus::Failed,
		allOk ? "Packages up to date." : "One or more packages failed to sync.");
	return allOk;
}