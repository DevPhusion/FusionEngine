#include "../../../Header Files/Core/Files/ProjectExportManager.h"
#include "../../../Header Files/Core/Files/FileManager.h"
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

	// Recursively copies a directory. Missing source directories are treated
	// as "nothing to do" rather than an error (e.g. a project with no .venv yet).
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

	// Dev-only build artifacts that shouldn't ship with the player.
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
		return dirName == ".vs" || dirName == "obj" || dirName == "x64" || dirName == "Debug";
	}

}

// ---------------------------------------------------------------------------
// Public entry points (called from the main/UI thread)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

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

	// ---- 1. Validate / prepare the export folder ----
	if (config.exportFolder.empty()) {
		outError = "No export folder specified.";
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	fs::path exportRoot(config.exportFolder);
	fs::create_directories(exportRoot, ec);
	if (ec) {
		outError = "Could not create export folder: " + ec.message();
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	if (!fs::is_empty(exportRoot, ec) || ec) {
		outError = "Export folder must be empty.";
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	std::string exeName = config.name.empty() ? "FusionPlayer" : config.name;

	// ---- 2. Locate the player runtime (built alongside the running editor exe) ----
	fs::path exeDir = GetCurrentExeDirectory();
	fs::path playerExeSource = exeDir / "FusionPlayer.exe";

	if (!fs::exists(playerExeSource, ec)) {
		outError = "Could not find FusionPlayer.exe next to the editor. Build the FusionPlayer project first.";
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	// ---- 3. Copy runtime files sitting next to the editor exe (DLLs, engine Resources, PythonStubs, etc.) ----
	SetStatus(ExportStage::CopyingRuntime, "Copying player runtime...");

	for (auto& entry : fs::directory_iterator(exeDir, ec)) {
		if (ec) break;

		const fs::path& srcPath = entry.path();
		fs::path name = srcPath.filename();

		if (entry.is_regular_file()) {
			if (name == "FusionPlayer.exe") continue; // handled separately below, gets renamed
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

	// Copy + rename the player exe to the export name
	fs::path targetExePath = exportRoot / (exeName + ".exe");
	fs::copy_file(playerExeSource, targetExePath, fs::copy_options::overwrite_existing, ec);
	if (ec) {
		outError = "Failed to copy FusionPlayer.exe: " + ec.message();
		Console::PrintError("ProjectExportManager: {}").Format(outError);
		return false;
	}

	// ---- 4. Copy project-specific data (resources + optional venv) ----
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

	{
		std::string err;
		if (!CopyDirectoryRecursive(projectDir / "resources", exportRoot / "resources", err)) {
			outError = err;
			Console::PrintError("ProjectExportManager: {}").Format(outError);
			return false;
		}

		// Only bundle the venv if this project actually uses scripting
		if (fs::exists(projectDir / ".venv", ec)) {
			SetStatus(ExportStage::CopyingProjectData, "Copying Python environment...");
			if (!CopyDirectoryRecursive(projectDir / ".venv", exportRoot / ".venv", err)) {
				outError = err;
				Console::PrintError("ProjectExportManager: {}").Format(outError);
				return false;
			}
		}
	}

	// ---- 5. Save the current project state as the shipped .fusion file ----
	SetStatus(ExportStage::SavingProject, "Saving project file...");

	fs::path projectFilePath = exportRoot / (exeName + ".fusion");
	FileManager::getInstance().SaveProjectToFile(projectFilePath.string());

	// ---- 6. Optional custom window icon ----
	if (!config.iconPath.empty() && config.iconPath != "Resources/Images/engineIcon.png") {
		fs::path iconDest = exportRoot / "Resources" / "Images" / "engineIcon.png";
		fs::create_directories(iconDest.parent_path(), ec);
		fs::copy_file(config.iconPath, iconDest, fs::copy_options::overwrite_existing, ec);
		if (ec) {
			// Non-fatal: player just falls back to the default icon
			Console::PrintError("ProjectExportManager: failed to copy custom icon, using default: {}").Format(ec.message());
		}
	}

	// ---- 7. Write export metadata alongside the build ----
	{
		fs::path metaPath = exportRoot / "export_info.json";
		std::ofstream out(metaPath);
		if (out.is_open()) {
			out << "{\n"
				<< "\t\"name\": \"" << exeName << "\",\n"
				<< "\t\"version\": \"" << config.version << "\",\n"
				<< "\t\"author\": \"" << config.author << "\"\n"
				<< "}\n";
		}
	}

	// ---- 8. Optionally zip the export folder, with the zip placed INSIDE it ----
	if (config.autoZipExport) {
		SetStatus(ExportStage::Zipping, "Compressing export...");

		fs::path finalZipPath = exportRoot / (exeName + ".zip");

		// Zip to a temp location first, then move it in. This avoids any risk of
		// the archive tool trying to read/include the zip file it's still writing.
		fs::path tempZipPath = fs::temp_directory_path(ec) / (exeName + "_export_tmp.zip");
		std::error_code zipEc;
		fs::remove(tempZipPath, zipEc);

		std::ostringstream cmd;
		cmd << "powershell -NoProfile -Command \"Compress-Archive -Path '"
			<< exportRoot.string() << "\\*' -DestinationPath '"
			<< tempZipPath.string() << "' -Force\"";

		int result = RunHiddenCommand(cmd.str());
		if (result != 0) {
			// Non-fatal: the unzipped export folder is still valid and usable
			Console::PrintError("ProjectExportManager: zip step failed (exit code {}); export folder is still available unzipped.").Format(result);
		}
		else {
			fs::rename(tempZipPath, finalZipPath, zipEc);
			if (zipEc) {
				// rename() can fail across drives/volumes — fall back to copy+delete
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