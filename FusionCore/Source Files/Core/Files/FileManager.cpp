#include "../../../Header Files/Core/Files/FileManager.h"
#include "../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../Header Files/Core/ObjectManager.h"
#include "../../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "../../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraints.h"
#include "../../../Header Files/Core/Scripting/PyBindings.h"
#include "../../../Header Files/Core/SceneManager.h"

namespace fs = std::filesystem;

FileManager::~FileManager() {
	ClearThumbnailCache();
}

ResourceIconType FileManager::ClassifyExtension(const std::string& extLower) {
	static const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".svg" };

	if (std::find(imageExts.begin(), imageExts.end(), extLower) != imageExts.end())
		return ResourceIconType::Image;

	if (extLower == ".py") {
		return ResourceIconType::Script;
	}
	if (extLower == ".fscene") {          
		return ResourceIconType::Scene;
	}

	return ResourceIconType::Unknown;
}

void FileManager::ProcessScriptInSubtree(const std::string& virtualPath, const std::function<void(const std::string&)>& callback) const {
	if (!IsDirectory(virtualPath)) {
		std::string ext = VirtualToAbsolute(virtualPath).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		if (ClassifyExtension(ext) == ResourceIconType::Script)
			callback(virtualPath);
		return;
	}

	for (auto& child : GetDirectoryContents(virtualPath))
		ProcessScriptInSubtree(child.virtualPath, callback);
}

void FileManager::UpdateScriptImportPath(const std::filesystem::path& previousRoot) {
	if (!Py_IsInitialized()) return;

	py::gil_scoped_acquire gil;
	py::object sysModule = py::module_::import("sys");
	py::list sysPath = sysModule.attr("path");

	if (!previousRoot.empty()) {
		std::string oldStr = previousRoot.string();
		py::list filtered;
		for (auto p : sysPath) {
			if (p.cast<std::string>() != oldStr) filtered.append(p);
		}
		sysModule.attr("path") = filtered;
		sysPath = sysModule.attr("path");
	}

	std::string newStr = resourcesRoot.string();
	bool alreadyPresent = false;
	for (auto p : sysPath) {
		if (p.cast<std::string>() == newStr) { alreadyPresent = true; break; }
	}
	if (!alreadyPresent) sysPath.append(newStr);

	py::dict sysModules = sysModule.attr("modules");
	std::vector<std::string> toDelete;
	for (auto item : sysModules) {
		std::string name = py::str(item.first).cast<std::string>();
		if (name == "scripts" || name.rfind("scripts.", 0) == 0) {
			toDelete.push_back(name);
		}
	}
	for (auto& name : toDelete) {
		sysModules.attr("pop")(name, py::none());
	}

	ResetDynamicComponentRegistries(); 
}

void FileManager::ScanForScripts(const std::string& virtualDir) {
	ProcessScriptInSubtree(virtualDir, [](const std::string& scriptVirtualPath) {
		ScriptManager::getInstance().RegisterScript(scriptVirtualPath);
		});
}

void FileManager::SetupResourcesFolder() {
	fs::path projectDir = currentProjectDirectory;
	fs::path resDir = projectDir / "resources";

	std::error_code ec;
	if (!fs::exists(resDir, ec)) {
		fs::create_directories(resDir, ec);
		if (fs::exists("Resources/Images/floorTiled.png")) {
			fs::copy("Resources/Images/floorTiled.png", resDir);
		}
	}

	fs::path previousRoot = resourcesRoot;
	resourcesRoot = resDir;
	ClearThumbnailCache();
	resourceGeneration++;

	ScriptManager::getInstance().ClearRegisteredScripts();
	ScanForScripts(GetRootVirtualPath());

	ScriptManager::getInstance().SetupPythonEnvironment(currentProjectDirectory);
	UpdateScriptImportPath(previousRoot);
}

std::filesystem::path FileManager::VirtualToAbsolute(const std::string& virtualPath) const {
	std::string rel = virtualPath;
	const std::string prefix = "res://";
	if (rel.rfind(prefix, 0) == 0)
		rel = rel.substr(prefix.size());

	if (rel.empty()) return resourcesRoot;
	return resourcesRoot / fs::path(rel);
}

std::string FileManager::AbsoluteToVirtual(const std::filesystem::path& absolutePath) const {
	std::error_code ec;
	fs::path rel = fs::relative(absolutePath, resourcesRoot, ec);
	if (ec) return "res://";

	std::string relStr = rel.generic_string();
	if (relStr == ".") return "res://";
	return "res://" + relStr;
}

bool FileManager::IsDirectory(const std::string& virtualPath) const {
	std::error_code ec;
	return fs::is_directory(VirtualToAbsolute(virtualPath), ec);
}

bool FileManager::VirtualPathExists(const std::string& virtualPath) const {
	std::error_code ec;
	return fs::exists(VirtualToAbsolute(virtualPath), ec);
}

std::vector<FileSystemEntry> FileManager::GetDirectoryContents(const std::string& virtualPath) const {
	std::vector<FileSystemEntry> result;

	fs::path absDir = VirtualToAbsolute(virtualPath);
	std::error_code ec;
	if (!fs::exists(absDir, ec) || !fs::is_directory(absDir, ec))
		return result;

	for (auto& entry : fs::directory_iterator(absDir, ec)) {
		if (ec) break;

		FileSystemEntry fe;
		fe.absolutePath = entry.path();
		fe.name = entry.path().filename().string();
		fe.isDirectory = entry.is_directory();
		fe.virtualPath = AbsoluteToVirtual(entry.path());

		if (fe.isDirectory) {
			fe.iconType = ResourceIconType::Folder;
		}
		else {
			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			fe.iconType = ClassifyExtension(ext);
		}

		result.push_back(std::move(fe));
	}

	std::sort(result.begin(), result.end(), [](const FileSystemEntry& a, const FileSystemEntry& b) {
		if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
		return a.name < b.name;
		});

	return result;
}

bool FileManager::CreateFolder(const std::string& parentVirtualPath, const std::string& folderName) {
	if (folderName.empty()) return false;

	fs::path target = VirtualToAbsolute(parentVirtualPath) / folderName;
	std::error_code ec;
	if (fs::exists(target, ec)) return false;

	return fs::create_directory(target, ec) && !ec;
}

bool FileManager::CreateScript(const std::string& parentVirtualPath, const std::string& scriptName) {
	if (scriptName.empty()) return false;

	std::string fileName = scriptName;
	const std::string ext = ".py";
	if (fileName.size() < ext.size() ||
		fileName.compare(fileName.size() - ext.size(), ext.size(), ext) != 0) {
		fileName += ext;
	}

	fs::path target = VirtualToAbsolute(parentVirtualPath) / fileName;
	std::error_code ec;
	if (fs::exists(target, ec)) return false;

	std::string className = fs::path(fileName).stem().string();
	if (!className.empty())
		className[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(className[0])));

	std::ofstream out(target);
	if (!out.is_open()) return false;

	out << "from fusion import *\n"
		<< "\n\n"
		<< "class " << className << "(Script):\n"
		<< "\tdef __init__(self):\n"
		<< "\t\tsuper().__init__()\n"
		<< "\n"
		<< "\tdef OnStart(self):\n"
		<< "\t\tpass\n"
		<< "\n"
		<< "\tdef Process(self, delta: float):\n"
		<< "\t\tpass\n";
	out.close();

	std::string virtualPath = AbsoluteToVirtual(target);
	ScriptManager::getInstance().RegisterScript(virtualPath);
	ScriptManager::getInstance().TryRegisterScriptAsComponent(virtualPath);
	return true;
}

bool FileManager::CreateScene(const std::string& parentVirtualPath, const std::string& sceneName) {
	if (sceneName.empty()) return false;

	std::string fileName = sceneName;
	const std::string ext = ".fscene";
	if (fileName.size() < ext.size() ||
		fileName.compare(fileName.size() - ext.size(), ext.size(), ext) != 0) {
		fileName += ext;
	}

	fs::path target = VirtualToAbsolute(parentVirtualPath) / fileName;
	std::error_code ec;
	if (fs::exists(target, ec)) return false;

	std::ofstream out(target, std::ios::binary);
	if (!out.is_open()) return false;

	BinaryWriter w(out);
	w.Write(SceneManager::getInstance().sceneMagicByte);
	w.Write(SceneManager::getInstance().sceneVersion);
	w.Write(static_cast<uint32_t>(0)); 
	w.Write(static_cast<uint32_t>(0)); 

	return w.Good();
}

bool FileManager::DeleteResource(const std::string& virtualPath) {
	fs::path target = VirtualToAbsolute(virtualPath);
	std::error_code ec;
	if (!fs::exists(target, ec)) return false;

	thumbnailCache.erase(target.string());

	std::vector<std::string> scriptsRemoved;
	ProcessScriptInSubtree(virtualPath, [&](const std::string& scriptVirtualPath) {
		scriptsRemoved.push_back(scriptVirtualPath);
		});

	fs::remove_all(target, ec);
	if (ec) return false;

	for (auto& sp : scriptsRemoved)
		ScriptManager::getInstance().UnregisterScript(sp);

	return true;
}

bool FileManager::ImportFile(const std::string& sourceAbsolutePath, const std::string& destVirtualDirectory) {
	fs::path source(sourceAbsolutePath);
	std::error_code ec;
	if (!fs::exists(source, ec) || fs::is_directory(source, ec)) return false;

	fs::path destDir = VirtualToAbsolute(destVirtualDirectory);
	fs::path dest = destDir / source.filename();

	fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
	if (ec) return false;

	std::string ext = dest.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	if (ClassifyExtension(ext) == ResourceIconType::Script) {
		std::string virtualPath = AbsoluteToVirtual(dest);
		ScriptManager::getInstance().RegisterScript(virtualPath);
		ScriptManager::getInstance().TryRegisterScriptAsComponent(virtualPath); 
	}

	return true;
}

bool FileManager::RenameResource(const std::string& virtualPath, const std::string& newName) {
	if (newName.empty()) return false;

	fs::path source = VirtualToAbsolute(virtualPath);
	fs::path dest = source.parent_path() / newName;

	std::error_code ec;
	if (fs::exists(dest, ec)) return false;

	thumbnailCache.erase(source.string());

	std::vector<std::string> scriptsBefore;
	ProcessScriptInSubtree(virtualPath, [&](const std::string& scriptVirtualPath) {
		scriptsBefore.push_back(scriptVirtualPath);
		});

	fs::rename(source, dest, ec);
	if (ec) return false;

	std::string newVirtualPath = AbsoluteToVirtual(dest);
	for (auto& oldScriptPath : scriptsBefore) {
		std::string suffix = oldScriptPath.substr(virtualPath.size()); 
		ScriptManager::getInstance().RenameRegisteredScript(oldScriptPath, newVirtualPath + suffix);
	}

	return true;
}

bool FileManager::MoveResource(const std::string& virtualPath, const std::string& destDirVirtualPath) {
	fs::path source = VirtualToAbsolute(virtualPath);
	fs::path destDir = VirtualToAbsolute(destDirVirtualPath);

	std::error_code ec;
	if (!fs::exists(source, ec) || !fs::exists(destDir, ec) || !fs::is_directory(destDir, ec))
		return false;

	if (source.parent_path() == destDir)
		return true;

	fs::path sourceCanon = fs::weakly_canonical(source, ec);
	fs::path destCanon = fs::weakly_canonical(destDir, ec);
	if (ec) return false;

	if (sourceCanon == destCanon) return false;

	std::string sourceStr = sourceCanon.generic_string();
	std::string destStr = destCanon.generic_string();
	if (destStr.size() > sourceStr.size() &&
		destStr.compare(0, sourceStr.size(), sourceStr) == 0 &&
		destStr[sourceStr.size()] == '/') {
		return false;
	}

	fs::path dest = destDir / source.filename();
	if (fs::exists(dest, ec)) return false;

	thumbnailCache.erase(source.string());

	std::vector<std::string> scriptsBefore;
	ProcessScriptInSubtree(virtualPath, [&](const std::string& scriptVirtualPath) {
		scriptsBefore.push_back(scriptVirtualPath);
		});

	fs::rename(source, dest, ec);
	if (ec) return false;

	std::string newVirtualPath = AbsoluteToVirtual(dest);
	for (auto& oldScriptPath : scriptsBefore) {
		std::string suffix = oldScriptPath.substr(virtualPath.size());
		ScriptManager::getInstance().RenameRegisteredScript(oldScriptPath, newVirtualPath + suffix);
	}

	return true;
}

unsigned int FileManager::GetOrLoadThumbnail(const std::filesystem::path& imageAbsolutePath) {
	std::string key = imageAbsolutePath.string();

	auto it = thumbnailCache.find(key);
	if (it != thumbnailCache.end()) return it->second;

	std::string ext = imageAbsolutePath.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	if (ext == ".svg") {
		thumbnailCache[key] = 0;
		return 0;
	}

	int width, height, channels;
	unsigned char* data = stbi_load(key.c_str(), &width, &height, &channels, 4);
	if (!data) {
		thumbnailCache[key] = 0;
		return 0;
	}

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);

	thumbnailCache[key] = texId;
	return texId;
}

void FileManager::ClearThumbnailCache() {
	for (auto& [path, texId] : thumbnailCache) {
		if (texId != 0) {
			GLuint id = texId;
			glDeleteTextures(1, &id);
		}
	}
	thumbnailCache.clear();
}

void FileManager::SaveProjectToFile(const std::string& path) {
	std::ofstream out(path, std::ios::binary);
	if (!out.is_open()) {
		Console::PrintError("SaveProject: failed to open file for writing {}").Format(path);
		return;
	}

	BinaryWriter w(out);
	w.Write(magicByte);
	w.Write(version);

	EngineManager::getInstance().SerializeEngineSettings(w);
	ProjectExportManager::getInstance().SerializeExportConfiguration(w);

	if (!w.Good())
		Console::Print("SaveProject: write failed, file may be incomplete: {}").Format(path);

	isProjectSaved = true;
}

void FileManager::LoadProjectFromFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open())
		Console::PrintError("LoadProject: failed to open file for reading {}").Format(path);
	LoadProjectFromStream(in);
}

void FileManager::LoadProjectFromMemory(const std::vector<uint8_t>& data) {
	std::string buffer(reinterpret_cast<const char*>(data.data()), data.size());
	std::istringstream in(buffer, std::ios::binary);
	LoadProjectFromStream(in);
}

void FileManager::LoadProjectFromStream(std::istream& in) {
	BinaryReader r(in);

	uint32_t magic = r.Read<uint32_t>();
	if (magic != magicByte) {
		Console::PrintError("LoadProject: file is not a valid .fusion file");
		return;
	}

	uint32_t ver = r.Read<uint32_t>();
	if (ver != version) {
		Console::PrintError("LoadProject: unsupported .fusion file version {}").Format((int)ver);
		return;
	}

	EngineManager::getInstance().DeserializeEngineSettings(r);
	ProjectExportManager::getInstance().DeserializeExportConfiguration(r);

	const std::string& mainScenePath = EngineManager::getInstance().EngineSettings.mainScenePath;

	std::error_code ec;
	if (!mainScenePath.empty() && fs::exists(mainScenePath, ec) && !ec) {
		SceneManager::getInstance().LoadSceneFromFile(mainScenePath);
	}
	else {
		if (!mainScenePath.empty())
			Console::Print("LoadProject: main scene {} not found, starting with an empty scene").Format(mainScenePath);
		SceneManager::getInstance().NewScene();
	}

	isProjectSaved = true;
}

void FileManager::NewProject() {
	EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<Object*> toRemove;
	toRemove.reserve(ObjectManager::getInstance().allObjects.size());
	for (auto& obj : ObjectManager::getInstance().allObjects)
		toRemove.push_back(obj.get());
	for (Object* obj : toRemove)
		ObjectManager::getInstance().RemoveObject(obj);
	ObjectManager::getInstance().allObjects.clear();

	currentProjectFile = "";
}

static void CollectSubtree(Object* obj, std::vector<Object*>& out) {
	out.push_back(obj);
	for (auto* child : obj->children) CollectSubtree(child, out);
}

std::vector<uint8_t> FileManager::SnapshotObjects(const std::vector<Object*>& roots) const {
	std::vector<Object*> all;
	for (auto* root : roots) CollectSubtree(root, all);

	std::ostringstream out(std::ios::binary);
	BinaryWriter w(out);
	w.Write(static_cast<uint32_t>(all.size()));
	for (auto* obj : all) obj->Serialize(w);

	std::string s = out.str();
	return std::vector<uint8_t>(s.begin(), s.end());
}

void FileManager::RestoreObjects(const std::vector<uint8_t>& data, const std::vector<uint64_t>& idsToRemove) {
	isRestoring = true;

	for (uint64_t id : idsToRemove) {
		ObjectManager::getInstance().RemoveObjectById(id);  
	}

	std::istringstream in(std::string(data.begin(), data.end()), std::ios::binary);
	BinaryReader r(in);
	std::vector<Object*> touched;

	uint32_t count = r.Read<uint32_t>();
	touched.reserve(count);
	for (uint32_t i = 0; i < count; i++) {
		uint64_t id = r.Read<uint64_t>();
		Object* existing = ObjectManager::getInstance().FindObjectById(id);
		if (existing) {
			existing->ApplyState(r);
			touched.push_back(existing);
		}
		else {
			auto obj = std::make_unique<Object>();
			obj->id = id;
			Object::ReserveID(id);
			obj->DeserializeBody(r);
			touched.push_back(obj.get());
			ObjectManager::getInstance().allObjects.push_back(std::move(obj));
		}
	}

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		obj->children.clear();
	}

	std::unordered_map<uint64_t, Object*> byId;
	for (auto& obj : ObjectManager::getInstance().allObjects) byId[obj->id] = obj.get();

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		if (obj->parentID != -1 && byId.count(obj->parentID)) {
			obj->SetParent(byId[obj->parentID]);
		}
		else {
			obj->parent = nullptr;
			obj->parentID = -1;
		}
	}

	for (Object* obj : touched) {          
		for (auto& c : obj->components) {
			c->PostLoad();
		}
	}

	isRestoring = false;
}

std::vector<uint8_t> FileManager::SnapshotConstraints() const {
	std::ostringstream out(std::ios::binary);
	BinaryWriter w(out);

	std::vector<Constraint*> toSave;
	for (Constraint* c : PhysicsEngine::getInstance().registeredPGSConstraints) {
		if (!c->isTemporary) toSave.push_back(c);
	}

	w.Write(static_cast<uint32_t>(toSave.size()));
	for (Constraint* c : toSave) {
		c->Serialize(w);   
	}

	std::string s = out.str();
	return std::vector<uint8_t>(s.begin(), s.end());
}

void FileManager::RestoreConstraints(const std::vector<uint8_t>& data) {
	std::istringstream in(std::string(data.begin(), data.end()), std::ios::binary);
	BinaryReader r(in);

	std::vector<Constraint*> current;
	for (Constraint* c : PhysicsEngine::getInstance().registeredPGSConstraints) {
		if (!c->isTemporary) current.push_back(c);
	}
	for (Constraint* c : current) {
		if (c->objectA.obj) {
			if (auto* cc = c->objectA.obj->GetComponent<ConstraintComponent>())
				cc->RemoveConstraint(c);
		}
	}

	uint32_t constraintCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < constraintCount; i++) {
		std::string name = r.ReadString();
		std::shared_ptr<Constraint> constraint = CreateConstraintFromName(name);
		uint64_t idA = r.Read<uint64_t>();
		uint64_t idB = r.Read<uint64_t>();

		Object* a = ObjectManager::getInstance().FindObjectById(idA);
		Object* b = ObjectManager::getInstance().FindObjectById(idB);

		if (!a) {
			constraint->Deserialize(r);   
			continue;
		}

		if (!a->HasComponent<ConstraintComponent>())
			a->AddComponent(std::make_unique<ConstraintComponent>(a));

		constraint->SetObjectA(PhysicsEngine::getInstance().GetBodyFromObject(a));
		constraint->SetObjectB(b ? PhysicsEngine::getInstance().GetBodyFromObject(b) : PhysicsBody());
		constraint->Deserialize(r);

		a->GetComponent<ConstraintComponent>()->AddConstraint(constraint);
	}
}