#include "../../../../Header Files/Core/Files/Export/ProjectExportManager.h"
#include "../../../../Header Files/Core/Files/FileManager.h"
#include "../../../../Header Files/Core/EngineManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

namespace {
	int RunHiddenCommand(const std::string& command) {
		STARTUPINFOA si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi{};

		std::vector<char> buffer(command.begin(), command.end());
		buffer.push_back('\0');

		BOOL ok = CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, FALSE,
			CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
		if (!ok) return -1;

		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 1;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return static_cast<int>(exitCode);
	}

	fs::path GetCurrentExeDirectory() {
		char exePathBuf[MAX_PATH];
		GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
		return fs::path(exePathBuf).parent_path();
	}

	bool CopyDirectoryRecursive(const fs::path& src, const fs::path& dst, std::string& error) {
		std::error_code ec;
		if (!fs::exists(src, ec)) return true;

		fs::create_directories(dst, ec);
		fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
		if (ec) {
			error = "Failed to copy '" + src.string() + "' to '" + dst.string() + "': " + ec.message();
			return false;
		}
		return true;
	}

	bool IsDevOnlyFile(const fs::path& filename) {
		static const std::vector<std::string> skipExact = {
			"FusionApp.exe", "FusionApp.pdb", "FusionPlayer.pdb", "FusionApp.ilk", "FusionPlayer.ilk"
		};
		static const std::vector<std::string> skipExtensions = {
			".pdb", ".ilk", ".exp", ".lib", ".iobj", ".ipdb", ".log"
		};

		std::string name = filename.filename().string();
		for (auto& s : skipExact) if (name == s) return true;

		std::string ext = filename.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		for (auto& s : skipExtensions) if (ext == s) return true;

		return false;
	}

	bool IsDevOnlyDirectory(const std::string& dirName) {
		return dirName == ".vs" || dirName == "obj" || dirName == "x64" || dirName == "Debug" ||
			dirName == "PythonStubs" || dirName == ".vscode" || dirName == "typings";
	}

	bool WritePackageFile(const fs::path& packPath, const std::vector<PackEntry>& entries, std::string& outError) {
		std::ofstream out(packPath, std::ios::binary);
		if (!out.is_open()) {
			outError = "Failed to open package file for writing: " + packPath.string();
			return false;
		}

		uint32_t magic = 0x4B415046, version = 2, entryCount = static_cast<uint32_t>(entries.size());
		out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
		out.write(reinterpret_cast<const char*>(&version), sizeof(version));
		out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

		PackageCrypto::Salt salt = PackageCrypto::GenerateRandomSalt();
		out.write(reinterpret_cast<const char*>(salt.data()), salt.size());
		std::array<uint8_t, 16> key = PackageCrypto::DeriveKey(salt);

		uint64_t runningOffset = 0;
		std::vector<uint64_t> offsets;
		for (auto& e : entries) { offsets.push_back(runningOffset); runningOffset += e.data.size(); }

		for (size_t i = 0; i < entries.size(); i++) {
			uint32_t pathLen = static_cast<uint32_t>(entries[i].virtualPath.size());
			uint64_t size = entries[i].data.size();
			out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
			out.write(entries[i].virtualPath.data(), pathLen);
			out.write(reinterpret_cast<const char*>(&offsets[i]), sizeof(uint64_t));
			out.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));
		}

		for (auto& e : entries) {
			std::vector<uint8_t> encrypted = e.data;
			PackageCrypto::XorBuffer(encrypted, key);
			out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
		}
		return out.good();
	}
}

bool ProjectExportManager::StartExport(const ExportConfiguration& config) {
	if (IsBusy()) return false;

	if (workerThread.joinable())
		workerThread.join();

	lastError.clear();
	backgroundSucceeded = false;
	SetStatus(ExportStage::CopyingRuntime, "Starting export...");

	workerThread = std::thread([this, config]() { RunExport(config); });
	return true;
}

void ProjectExportManager::Update() {
	ExportStage s = stage.load();
	if ((s == ExportStage::Done || s == ExportStage::Failed) && workerThread.joinable()) {
		workerThread.join();
	}
}

bool ProjectExportManager::IsBusy() const {
	ExportStage s = stage.load();
	return s == ExportStage::CopyingRuntime || s == ExportStage::CopyingProjectData ||
		s == ExportStage::SavingProject || s == ExportStage::Zipping;
}

std::string ProjectExportManager::GetStatusMessage() const {
	std::lock_guard<std::mutex> lock(statusMutex);
	return statusMessage;
}

void ProjectExportManager::SetStatus(ExportStage newStage, const std::string& message) {
	{
		std::lock_guard<std::mutex> lock(statusMutex);
		statusMessage = message;
	}
	stage.store(newStage);
}

void ProjectExportManager::RunExport(ExportConfiguration config) {
	std::string error;
	bool ok = DoExport(config, error);

	backgroundSucceeded = ok;
	if (ok) {
		SetStatus(ExportStage::Done, "Export complete.");
	}
	else {
		lastError = error;
		SetStatus(ExportStage::Failed, error);
	}
}

bool ProjectExportManager::DoExport(const ExportConfiguration& config, std::string& outError) {
	std::error_code ec;

	if (config.exportFolder.empty()) {
		outError = "No export folder specified.";
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	std::vector<uint8_t> iconBytes;
	bool hasCustomIcon = false;
	if (!config.iconPath.empty() && config.iconPath != "Resources/Images/engineIcon.png") {
		if (fs::exists(config.iconPath, ec)) {
			std::ifstream iconIn(config.iconPath, std::ios::binary);
			if (iconIn.is_open()) {
				iconBytes.assign((std::istreambuf_iterator<char>(iconIn)), std::istreambuf_iterator<char>());
				hasCustomIcon = !iconBytes.empty();
			}
		}
		else {
			Console::PrintError("ProjectExportManager: custom icon path does not exist: {} — using default.").Format(config.iconPath);
		}
	}

	fs::path exportRoot(config.exportFolder);
	fs::create_directories(exportRoot, ec);
	if (ec) {
		outError = "Could not create export folder: " + ec.message();
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	if (fs::exists(exportRoot / "export_info.json", ec)) {
		fs::remove_all(exportRoot / "resources", ec);
		fs::remove(exportRoot / "data.pack", ec);
		for (auto& entry : fs::directory_iterator(exportRoot, ec)) {
			if (ec) break;
			if (entry.path().extension() == ".fusion") {
				fs::remove(entry.path(), ec);
			}
		}
	}

	std::string exeName = config.name.empty() ? "FusionPlayer" : config.name;

	fs::path exeDir = GetCurrentExeDirectory();
	fs::path playerExeSource = exeDir / "FusionPlayer.exe";

	if (!fs::exists(playerExeSource, ec)) {
		outError = "Could not find FusionPlayer.exe next to the editor. Build the FusionPlayer project first.";
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	SetStatus(ExportStage::CopyingRuntime, "Copying player runtime...");

	for (auto& entry : fs::directory_iterator(exeDir, ec)) {
		if (ec) break;

		const fs::path& srcPath = entry.path();
		fs::path name = srcPath.filename();

		if (entry.is_regular_file()) {
			if (name == "FusionPlayer.exe") continue; 
			if (IsDevOnlyFile(name)) continue;

			fs::copy_file(srcPath, exportRoot / name, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				outError = "Failed to copy runtime file '" + name.string() + "': " + ec.message();
				Console::PrintError("ProjectExportManager: {}").Format(outError);
				return false;
			}
		}
		else if (entry.is_directory()) {
			std::string dirName = name.string();
			if (IsDevOnlyDirectory(dirName)) continue;

			std::string err;
			if (!CopyDirectoryRecursive(srcPath, exportRoot / name, err)) {
				outError = err;
				Console::PrintError("ProjectExportManager: {}").Format(outError);
				return false;
			}
		}
	}
	
	if (hasCustomIcon) {
		fs::path iconDest = exportRoot / "Resources" / "Images" / "engineIcon.png";
		fs::create_directories(iconDest.parent_path(), ec);
		std::ofstream iconOut(iconDest, std::ios::binary);
		if (iconOut.is_open()) {
			iconOut.write(reinterpret_cast<const char*>(iconBytes.data()), iconBytes.size());
		}
		else {
			Console::PrintError("ProjectExportManager: failed to write custom icon to export folder.");
		}
	}

	fs::path targetExePath = exportRoot / (exeName + ".exe");
	fs::copy_file(playerExeSource, targetExePath, fs::copy_options::overwrite_existing, ec);
	if (ec) {
		outError = "Failed to copy FusionPlayer.exe: " + ec.message();
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	SetStatus(ExportStage::CopyingProjectData, "Copying project resources...");

	fs::path projectDir = FileManager::getInstance().currentProjectDirectory;
	if (projectDir.empty() && !FileManager::getInstance().currentProjectFile.empty()) {
		projectDir = fs::path(FileManager::getInstance().currentProjectFile).parent_path();
	}
	if (projectDir.empty()) {
		outError = "No project is currently open — nothing to export.";
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	std::vector<PackEntry> packEntries;

	{
		fs::path resourcesSrc = projectDir / "resources";
		fs::path resourcesDst = exportRoot / "resources";
		fs::create_directories(resourcesDst, ec);

		if (fs::exists(resourcesSrc, ec)) {
			for (auto& entry : fs::recursive_directory_iterator(resourcesSrc, ec)) {
				if (ec) break;

				bool inPycache = false;
				for (auto& part : entry.path()) {
					if (part == "__pycache__") { inPycache = true; break; }
				}
				if (inPycache) continue;

				if (!entry.is_regular_file()) continue;

				fs::path relative = fs::relative(entry.path(), resourcesSrc, ec);
				if (ec) continue;

				if (entry.path().extension() == ".py") {
					std::ifstream scriptIn(entry.path(), std::ios::binary);
					std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(scriptIn)), std::istreambuf_iterator<char>());
					packEntries.push_back({ "res://" + relative.generic_string(), std::move(bytes) });
				}
				else if (entry.path().extension() == ".fscene") {
					std::ifstream sceneIn(entry.path(), std::ios::binary);
					std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(sceneIn)), std::istreambuf_iterator<char>());
					packEntries.push_back({ "res://" + relative.generic_string(), std::move(bytes) });
				}
				else {
					fs::path dst = resourcesDst / relative;
					fs::create_directories(dst.parent_path(), ec);
					fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
				}
			}
		}

		if (fs::exists(projectDir / ".venv", ec)) {
			SetStatus(ExportStage::CopyingProjectData, "Copying Python environment...");
			std::string err;
			if (!CopyDirectoryRecursive(projectDir / ".venv", exportRoot / ".venv", err)) {
				outError = err;
				Console::PrintError("ProjectExportManager: {}").Format(outError);
				return false;
			}
		}
	}

	SetStatus(ExportStage::SavingProject, "Packaging project file...");
	{
		std::string& liveMainScene = EngineManager::getInstance().EngineSettings.mainScenePath;
		std::string originalMainScene = liveMainScene;
		if (!originalMainScene.empty()) {
			liveMainScene = FileManager::getInstance().AbsoluteToVirtual(originalMainScene);
		}

		fs::path tempFusionPath = fs::temp_directory_path(ec) / (exeName + "_export_tmp.fusion");
		FileManager::getInstance().SaveProjectToFile(tempFusionPath.string());

		liveMainScene = originalMainScene;   

		std::ifstream fusionIn(tempFusionPath, std::ios::binary);
		std::vector<uint8_t> fusionBytes((std::istreambuf_iterator<char>(fusionIn)), std::istreambuf_iterator<char>());
		fusionIn.close();
		fs::remove(tempFusionPath, ec);

		packEntries.push_back({ "__project__", std::move(fusionBytes) });
	}

	{
		std::string err;
		if (!WritePackageFile(exportRoot / "data.pack", packEntries, err)) {
			outError = err;
			Console::PrintError("ProjectExportManager: {}").Format(outError);
			return false;
		}
	}

	if (config.autoZipExport) {
		SetStatus(ExportStage::Zipping, "Compressing export...");

		fs::path finalZipPath = exportRoot / (exeName + ".zip");
		fs::path tempZipPath = fs::temp_directory_path(ec) / (exeName + "_export_tmp.zip");
		std::error_code zipEc;
		fs::remove(tempZipPath, zipEc);

		std::ostringstream cmd;
		cmd << "powershell -NoProfile -Command \"Compress-Archive -Path '"
			<< exportRoot.string() << "\\*' -DestinationPath '"
			<< tempZipPath.string() << "' -Force\"";

		int result = RunHiddenCommand(cmd.str());
		if (result != 0) {
			Console::PrintError("ProjectExportManager: zip step failed (exit code {}); export folder is still available unzipped.").Format(result);
		}
		else {
			fs::rename(tempZipPath, finalZipPath, zipEc);
			if (zipEc) {
				fs::copy_file(tempZipPath, finalZipPath, fs::copy_options::overwrite_existing, zipEc);
				fs::remove(tempZipPath, zipEc);
			}

			if (zipEc) {
				Console::PrintError("ProjectExportManager: failed to move zip into export folder: {}").Format(zipEc.message());
			}
			else {
				Console::Print("ProjectExportManager: zipped export to {}").Format(finalZipPath.string());
			}
		}
	}

	Console::Print("ProjectExportManager: successfully exported '{}' to {}").Format(exeName, exportRoot.string());
	return true;
}

void ProjectExportManager::SerializeExportConfiguration(BinaryWriter& w) {
	w.WriteString(exportConfig.exportFolder);
	w.WriteString(exportConfig.name);
	w.WriteString(exportConfig.version);
	w.WriteString(exportConfig.iconPath);
	w.WriteString(exportConfig.author);
	w.Write(exportConfig.autoZipExport);
}

void ProjectExportManager::DeserializeExportConfiguration(BinaryReader& r) {
	exportConfig.exportFolder = r.ReadString();
	exportConfig.name = r.ReadString();
	exportConfig.version = r.ReadString();
	exportConfig.iconPath = r.ReadString();
	exportConfig.author = r.ReadString();
	exportConfig.autoZipExport = r.Read<bool>();
}
