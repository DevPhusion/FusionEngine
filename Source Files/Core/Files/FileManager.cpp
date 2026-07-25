#include "../../../Header Files/Core/Files/FileManager.h"
#include "../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../Header Files/Core/ObjectManager.h"
#include "../../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "../../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraints.h"

namespace fs = std::filesystem;

FileManager::~FileManager() {
	ClearThumbnailCache();
}

ResourceIconType FileManager::ClassifyExtension(const std::string& extLower) {
	static const std::vector<std::string> imageExts = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".svg" };

	if (std::find(imageExts.begin(), imageExts.end(), extLower) != imageExts.end())
		return ResourceIconType::Image;

	return ResourceIconType::Unknown;
}

void FileManager::SetupResourcesFolder() {
	fs::path projectDir = currentProjectDirectory;
	fs::path resDir = projectDir / "resources";

	std::error_code ec;
	if (!fs::exists(resDir, ec)) {
		fs::create_directories(resDir, ec);
		fs::copy("floorTiled.png", resDir);
	}

	resourcesRoot = resDir;
	ClearThumbnailCache();
	resourceGeneration++;

	ScriptManager::getInstance().SetupPythonEnvironment(currentProjectDirectory);
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

bool FileManager::DeleteResource(const std::string& virtualPath) {
	fs::path target = VirtualToAbsolute(virtualPath);
	std::error_code ec;
	if (!fs::exists(target, ec)) return false;

	thumbnailCache.erase(target.string());
	fs::remove_all(target, ec);
	return !ec;
}

bool FileManager::ImportFile(const std::string& sourceAbsolutePath, const std::string& destVirtualDirectory) {
	fs::path source(sourceAbsolutePath);
	std::error_code ec;
	if (!fs::exists(source, ec) || fs::is_directory(source, ec)) return false;

	fs::path destDir = VirtualToAbsolute(destVirtualDirectory);
	fs::path dest = destDir / source.filename();

	fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
	return !ec;
}

bool FileManager::RenameResource(const std::string& virtualPath, const std::string& newName) {
	if (newName.empty()) return false;

	fs::path source = VirtualToAbsolute(virtualPath);
	fs::path dest = source.parent_path() / newName;

	std::error_code ec;
	if (fs::exists(dest, ec)) return false;

	thumbnailCache.erase(source.string());
	fs::rename(source, dest, ec);
	return !ec;
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
	fs::rename(source, dest, ec);
	return !ec;
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
	if (!out.is_open())
		throw std::runtime_error("SaveProjectToFile: failed to open file for writing: " + path);

	BinaryWriter w(out);

	w.Write(magicByte);
	w.Write(version);

	auto& objects = ObjectManager::getInstance().allObjects;

	std::vector<Object*> toSave;
	toSave.reserve(objects.size());
	for (auto& obj : objects)
	{
		if (obj->hidden) continue;
		toSave.push_back(obj.get());
	}

	w.Write(static_cast<uint32_t>(toSave.size()));
	for (Object* obj : toSave)
		obj->Serialize(w);

	std::vector<Constraint*> constraintToSave;
	constraintToSave.reserve(PhysicsEngine::getInstance().registeredPGSConstraints.size());
	for (Constraint* c : PhysicsEngine::getInstance().registeredPGSConstraints) {
		if (!c->isTemporary) constraintToSave.push_back(c);
	}

	w.Write(static_cast<uint32_t>(constraintToSave.size()));
	for (Constraint* c : constraintToSave) {
		c->Serialize(w);
	}

	if (!w.Good())
		throw std::runtime_error("SaveProjectToFile: write failed, file may be incomplete: " + path);

	isSaved = true;
}

void FileManager::LoadProjectFromFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open())
		throw std::runtime_error("LoadProjectFromFile: failed to open file for reading: " + path);

	BinaryReader r(in);

	uint32_t magic = r.Read<uint32_t>();
	if (magic != magicByte)
		throw std::runtime_error("LoadProjectFromFile: file is not a valid .fusion file: " + path);

	uint32_t ver = r.Read<uint32_t>();
	if (ver != version)
		throw std::runtime_error("LoadProjectFromFile: unsupported .fusion file version " + std::to_string(ver));

	uint32_t objectCount = r.Read<uint32_t>();

	EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<Object*> toRemove;
	toRemove.reserve(ObjectManager::getInstance().allObjects.size());
	for (auto& obj : ObjectManager::getInstance().allObjects)
		toRemove.push_back(obj.get());
	for (Object* obj : toRemove)
		ObjectManager::getInstance().RemoveObject(obj);
	ObjectManager::getInstance().allObjects.clear();
	PhysicsEngine::getInstance().registeredPGSConstraints.clear();

	for (uint32_t i = 0; i < objectCount; i++)
	{
		auto obj = std::make_unique<Object>();
		obj->Deserialize(r);
		ObjectManager::getInstance().allObjects.push_back(std::move(obj));
	}

	std::unordered_map<uint64_t, Object*> objectsById;
	for (auto& obj : ObjectManager::getInstance().allObjects)
		objectsById[obj->id] = obj.get();

	uint32_t constraintCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < constraintCount; i++)
	{
		std::string name = r.ReadString();
		std::shared_ptr<Constraint> constraint = CreateConstraintFromName(name);
		uint64_t idA = r.Read<uint64_t>();
		uint64_t idB = r.Read<uint64_t>();

		Object* a = objectsById.count(idA) ? objectsById[idA] : nullptr;
		Object* b = objectsById.count(idB) ? objectsById[idB] : nullptr;
		if (!a) {
			constraint->Deserialize(r);
			continue;
		}

		if (!a->HasComponent<ConstraintComponent>())
			a->AddComponent(std::make_unique<ConstraintComponent>(a));
		constraint->SetObjectA(PhysicsEngine::getInstance().GetBodyFromObject(a));
		constraint->SetObjectB(PhysicsEngine::getInstance().GetBodyFromObject(b));
		constraint->Deserialize(r);
		a->GetComponent<ConstraintComponent>()->AddConstraint(constraint);
		constraint->ProcessConstraintDisplay();
	}

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& c : obj->components) {
			c->PostLoad();
		}
	}

	isSaved = true;
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
	isSaved = false;
}