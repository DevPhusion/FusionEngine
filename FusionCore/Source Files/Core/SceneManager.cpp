#include "../../Header Files/Core/SceneManager.h"
#include "../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraints.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"
#include "../../Header Files/Core/Files/Export/PackageReader.h"
#include "../../Header Files/Core/Scripting/ScriptManager.h"

namespace fs = std::filesystem;

SceneManager::SceneManager() {
	OpenScene initial;
	initial.uid = AllocateSceneUid();
	openScenes.push_back(std::move(initial));
	activeIndex = 0;
}

std::string SceneManager::ToComparablePath(const std::string& path) const {
	if (path.empty()) return path;

	if (path.rfind("res://", 0) == 0) {
		if (!EngineManager::getInstance().isPlayer)
			return FileManager::getInstance().VirtualToAbsolute(path).string();
	}

	return path;
}

std::string SceneManager::NormalizeToVirtualPath(const std::string& path) const {
	if (path.empty() || path.rfind("res://", 0) == 0) return path;

	if (EngineManager::getInstance().isPlayer) {
		Console::PrintError("SceneManager: scene reference '{}' is not a res:// path and can't be resolved in an exported build.").Format(path);
		return path;
	}

	return FileManager::getInstance().AbsoluteToVirtual(path);
}

void SceneManager::ClearLiveScene() {
	EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<Object*> toRemove;
	toRemove.reserve(ObjectManager::getInstance().allObjects.size());
	for (auto& obj : ObjectManager::getInstance().allObjects)
		toRemove.push_back(obj.get());
	for (Object* obj : toRemove)
		ObjectManager::getInstance().RemoveObject(obj);

	ObjectManager::getInstance().allObjects.clear();
	PhysicsEngine::getInstance().registeredPGSConstraints.clear();
}

void SceneManager::SwapActiveWithLive() {
	if (activeIndex < 0) return;
	DeactivateLiveScene();

	OpenScene& active = openScenes[activeIndex];

	std::swap(active.objects, ObjectManager::getInstance().allObjects);
	std::swap(active.constraints, PhysicsEngine::getInstance().registeredPGSConstraints);
	active.selectedObject = EditorManager::getInstance().selectedObject;
}

void SceneManager::SwapLiveWithScene(int index) {
	OpenScene& next = openScenes[index];

	std::swap(next.objects, ObjectManager::getInstance().allObjects);
	std::swap(next.constraints, PhysicsEngine::getInstance().registeredPGSConstraints);

	activeIndex = index;
	focusRequestIndex = index;
	EditorManager::getInstance().SetSelectedObject(next.selectedObject);

	RefreshSceneInstances(ObjectManager::getInstance().allObjects, PhysicsEngine::getInstance().registeredPGSConstraints);

	ActivateLiveScene();
}

bool SceneManager::ParseSceneObjects(const std::string& path, std::vector<std::unique_ptr<Object>>& outObjects, uint32_t& outObjectCount) {
	std::unique_ptr<std::istream> in;
	std::string packedBuffer;

	if (EngineManager::getInstance().isPlayer) {
		std::string virtualPath = path.rfind("res://", 0) == 0
			? path
			: FileManager::getInstance().AbsoluteToVirtual(path);

		const std::vector<uint8_t>* packed = PackageReader::getInstance().Get(virtualPath);
		if (!packed) {
			Console::PrintError("AddScene: scene not found in package: {}").Format(virtualPath);
			return false;
		}

		packedBuffer.assign(reinterpret_cast<const char*>(packed->data()), packed->size());
		in = std::make_unique<std::istringstream>(packedBuffer, std::ios::binary);
	}
	else {
		std::string absPath = path.rfind("res://", 0) == 0
			? FileManager::getInstance().VirtualToAbsolute(path).string()
			: path;

		auto fileStream = std::make_unique<std::ifstream>(absPath, std::ios::binary);
		if (!fileStream->is_open()) {
			Console::PrintError("AddScene: failed to open file for reading {}").Format(absPath);
			return false;
		}
		in = std::move(fileStream);
	}

	BinaryReader r(*in);

	uint32_t magic = r.Read<uint32_t>();
	if (magic != sceneMagicByte) {
		Console::PrintError("AddScene: file is not a valid .fscene file");
		return false;
	}

	uint32_t ver = r.Read<uint32_t>();
	if (ver != sceneVersion) {
		Console::PrintError("AddScene: unsupported .fscene file version {}").Format((int)ver);
		return false;
	}

	uint32_t objectCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < objectCount; i++) {
		auto obj = std::make_unique<Object>();
		obj->Deserialize(r);
		outObjects.push_back(std::move(obj));
	}

	std::unordered_map<uint64_t, Object*> objectsById;
	for (auto& obj : outObjects)
		objectsById[obj->id] = obj.get();

	for (auto& obj : outObjects) {
		if (obj->parentID != -1 && objectsById.count(static_cast<uint64_t>(obj->parentID))) {
			obj->SetParent(objectsById[static_cast<uint64_t>(obj->parentID)]);
		}
		else {
			obj->parent = nullptr;
			obj->parentID = -1;
		}
	}

	std::vector<Constraint*> scratchConstraints;
	std::swap(scratchConstraints, PhysicsEngine::getInstance().registeredPGSConstraints);

	uint32_t constraintCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < constraintCount; i++) {
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
	}

	for (Constraint* c : PhysicsEngine::getInstance().registeredPGSConstraints)
		scratchConstraints.push_back(c);
	PhysicsEngine::getInstance().registeredPGSConstraints = std::move(scratchConstraints);

	outObjectCount = objectCount;
	return true;
}

int SceneManager::FindSceneByPath(const std::string& path) const {
	if (path.empty()) return -1;

	std::string target = ToComparablePath(path);
	for (int i = 0; i < (int)openScenes.size(); i++) {
		if (ToComparablePath(openScenes[i].filePath) == target) return i;
	}
	return -1;
}

void SceneManager::SwitchToScene(int index) {
	if (index < 0 || index >= (int)openScenes.size() || index == activeIndex) return;

	SwapActiveWithLive();
	SwapLiveWithScene(index);
}

int SceneManager::OpenSceneTab(const std::string& path) {
	int existing = FindSceneByPath(path);
	if (existing != -1) {
		SwitchToScene(existing);
		return existing;
	}

	SwapActiveWithLive();

	OpenScene scene;
	scene.uid = AllocateSceneUid();
	scene.filePath = path;
	scene.displayName = fs::path(path).filename().string();
	openScenes.push_back(std::move(scene));

	int newIndex = (int)openScenes.size() - 1;
	SwapLiveWithScene(newIndex);
	LoadSceneFromFile(path);

	return newIndex;
}

int SceneManager::NewSceneTab() {
	SwapActiveWithLive();

	OpenScene scene;
	scene.uid = AllocateSceneUid();
	openScenes.push_back(std::move(scene));

	int newIndex = (int)openScenes.size() - 1;
	SwapLiveWithScene(newIndex);

	return newIndex;
}

int SceneManager::ConsumeFocusRequest() {
	int r = focusRequestIndex;
	focusRequestIndex = -1;
	return r;
}

void SceneManager::CloseSceneTab(int index, bool discardUnsaved) {
	if (index < 0 || index >= (int)openScenes.size()) return;
	if (!discardUnsaved && openScenes[index].isDirty) return;

	bool closingActive = (index == activeIndex);

	if (closingActive) {
		ClearLiveScene();
		activeIndex = -1;
	}
	else {
		for (auto& obj : openScenes[index].objects) {
			obj->OnDelete();
		}
	}

	openScenes.erase(openScenes.begin() + index);

	if (openScenes.empty()) {
		OpenScene scene;
		scene.uid = AllocateSceneUid();
		openScenes.push_back(std::move(scene));
		activeIndex = 0;
		return;
	}

	if (closingActive) {
		int newActive = std::min(index, (int)openScenes.size() - 1);
		SwapLiveWithScene(newActive);
	}
	else if (activeIndex > index) {
		activeIndex--;
	}
}

void SceneManager::SaveScene(const std::string& path) {
	std::ofstream out(path, std::ios::binary);
	if (!out.is_open()) {
		Console::PrintError("SaveScene: failed to open file for writing {}").Format(path);
		return;
	}

	BinaryWriter w(out);
	w.Write(sceneMagicByte);
	w.Write(sceneVersion);

	auto& objects = ObjectManager::getInstance().allObjects;

	std::vector<Object*> toSave;
	toSave.reserve(objects.size());
	for (auto& obj : objects) {
		if (IsInsideSceneInstance(obj.get())) continue;   
		toSave.push_back(obj.get());
	}

	w.Write(static_cast<uint32_t>(toSave.size()));
	for (Object* obj : toSave)
		obj->Serialize(w);

	std::vector<Constraint*> constraintToSave;
	constraintToSave.reserve(PhysicsEngine::getInstance().registeredPGSConstraints.size());
	for (Constraint* c : PhysicsEngine::getInstance().registeredPGSConstraints) {
		if (c->isTemporary) continue;
		if (c->objectA.obj && IsInsideSceneInstance(c->objectA.obj)) continue;   
		constraintToSave.push_back(c);
	}

	w.Write(static_cast<uint32_t>(constraintToSave.size()));
	for (Constraint* c : constraintToSave)
		c->Serialize(w);

	if (!w.Good()) {
		Console::PrintError("SaveScene: write failed, file may be incomplete: {}").Format(path);
		return;
	}

	OpenScene& active = openScenes[activeIndex];
	active.filePath = path;
	active.displayName = fs::path(path).filename().string();
	active.isDirty = false;
}

bool SceneManager::SaveActiveScene() {
	if (activeIndex < 0) return false;
	const std::string& path = openScenes[activeIndex].filePath;
	if (path.empty()) return false;

	SaveScene(path);
	return true;
}

void SceneManager::LoadSceneFromFile(const std::string& path) {
	std::unique_ptr<std::istream> in;
	std::string packedBuffer;
	std::string resolvedPath = path;

	if (EngineManager::getInstance().isPlayer) {
		std::string virtualPath = path.rfind("res://", 0) == 0
			? path
			: FileManager::getInstance().AbsoluteToVirtual(path);

		const std::vector<uint8_t>* packed = PackageReader::getInstance().Get(virtualPath);
		if (!packed) {
			Console::PrintError("LoadScene: scene not found in package: {}").Format(virtualPath);
			return;
		}

		packedBuffer.assign(reinterpret_cast<const char*>(packed->data()), packed->size());
		in = std::make_unique<std::istringstream>(packedBuffer, std::ios::binary);
		resolvedPath = virtualPath;
	}
	else {
		std::string absPath = path.rfind("res://", 0) == 0
			? FileManager::getInstance().VirtualToAbsolute(path).string()
			: path;

		auto fileStream = std::make_unique<std::ifstream>(absPath, std::ios::binary);
		if (!fileStream->is_open()) {
			Console::PrintError("LoadScene: failed to open file for reading {}").Format(absPath);
			return;
		}
		in = std::move(fileStream);
		resolvedPath = absPath;
	}

	BinaryReader r(*in);

	uint32_t magic = r.Read<uint32_t>();
	if (magic != sceneMagicByte) {
		Console::PrintError("LoadScene: file is not a valid .fscene file");
		return;
	}

	uint32_t ver = r.Read<uint32_t>();
	if (ver != sceneVersion) {
		Console::PrintError("LoadScene: unsupported .fscene file version {}").Format((int)ver);
		return;
	}

	ClearLiveScene();

	uint32_t objectCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < objectCount; i++) {
		auto obj = std::make_unique<Object>();
		obj->Deserialize(r);
		ObjectManager::getInstance().allObjects.push_back(std::move(obj));
	}

	std::unordered_map<uint64_t, Object*> objectsById;
	for (auto& obj : ObjectManager::getInstance().allObjects)
		objectsById[obj->id] = obj.get();

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		if (obj->parentID != -1 && objectsById.count(static_cast<uint64_t>(obj->parentID))) {
			obj->SetParent(objectsById[static_cast<uint64_t>(obj->parentID)]);
		}
		else {
			obj->parent = nullptr;
			obj->parentID = -1;
		}
	}

	uint32_t constraintCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < constraintCount; i++) {
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
	}

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& c : obj->components) {
			c->PostLoad();
		}
	}

	Console::Print("LoadScene: successfully loaded scene with {} objects").Format(objectCount);

	std::vector<Object*> rootsToExpand;
	for (auto& obj : ObjectManager::getInstance().allObjects) {
		if (obj->isSceneRoot) rootsToExpand.push_back(obj.get());
	}

	std::vector<std::string> expansionStack;
	expansionStack.push_back(resolvedPath);   
	for (Object* root : rootsToExpand) {
		ExpandSceneRoot(root, ObjectManager::getInstance().allObjects,
			PhysicsEngine::getInstance().registeredPGSConstraints, false, expansionStack);
	}

	ActivateLiveScene();

	OpenScene& active = openScenes[activeIndex];
	active.filePath = resolvedPath;
	active.displayName = fs::path(resolvedPath).filename().string();
	active.isDirty = false;
}

void SceneManager::NewScene() {
	ClearLiveScene();

	OpenScene& active = openScenes[activeIndex];
	active.filePath.clear();
	active.displayName = "New Scene.fscene";
	active.isDirty = false;
}

Object* SceneManager::AddScene(const std::string& path, Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();

	std::string virtualPath = NormalizeToVirtualPath(path);

	std::unique_ptr<Object> rootObj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	rootObj->AddComponent(std::make_unique<EditorRenderComponent>(rootObj.get(), rootObj->shader, "Resources/Images/Object.png", 0.075f));
	rootObj->AddComponent(std::make_unique<TransformComponent>(rootObj.get(), rootObj->shader, rootObj->GetComponent<EditorRenderComponent>()->GetCenter()));
	rootObj->AddComponent(std::make_unique<MouseInteractComponent>(rootObj.get(), false));

	rootObj->name = ObjectManager::getInstance().GenerateUniqueName(std::filesystem::path(virtualPath).stem().string(), nullptr);
	rootObj->isSceneRoot = true;
	rootObj->sourceScenePath = virtualPath;

	for (auto& c : rootObj->components)
		c->Activate();

	Object* rootRaw = rootObj.get();

	rootRaw->addedToScene = true;
	rootRaw->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(rootRaw);
	ObjectManager::getInstance().allObjects.push_back(std::move(rootObj));

	std::vector<std::string> expansionStack;
	if (!ExpandSceneRoot(rootRaw, ObjectManager::getInstance().allObjects,
		PhysicsEngine::getInstance().registeredPGSConstraints, true, expansionStack)) {
		Console::PrintError("AddScene: failed to expand {}").Format(virtualPath);
	}

	return rootRaw;
}

void SceneManager::RemoveScene(Object* root) {
	if (!root) return;

	std::vector<Object*> subtree;
	std::function<void(Object*)> collect = [&](Object* o) {
		subtree.push_back(o);
		for (Object* child : o->children) collect(child);
		};
	collect(root);

	for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
		ObjectManager::getInstance().RemoveObject(*it);
	}
}

bool SceneManager::IsInsideSceneInstance(Object* obj) const {
	Object* p = obj->parent;
	while (p) {
		if (p->isSceneRoot) return true;
		p = p->parent;
	}
	return false;
}

void SceneManager::ActivateLiveScene() {
	if (activeIndex < 0) return;

	bool wasDirty = openScenes[activeIndex].isDirty;

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& c : obj->components) {
			c->Activate();
		}
	}

	openScenes[activeIndex].isDirty = wasDirty;
}

bool SceneManager::ParseTopLevelSceneObjects(const std::string& path, std::vector<std::unique_ptr<Object>>& outObjects,
	std::vector<Constraint*>& outConstraints, uint32_t& outObjectCount) {
	std::unique_ptr<std::istream> in;
	std::string packedBuffer;

	if (EngineManager::getInstance().isPlayer) {
		std::string virtualPath = path.rfind("res://", 0) == 0
			? path
			: FileManager::getInstance().AbsoluteToVirtual(path);

		const std::vector<uint8_t>* packed = PackageReader::getInstance().Get(virtualPath);
		if (!packed) {
			Console::PrintError("ParseTopLevelSceneObjects: scene not found in package: {}").Format(virtualPath);
			return false;
		}

		packedBuffer.assign(reinterpret_cast<const char*>(packed->data()), packed->size());
		in = std::make_unique<std::istringstream>(packedBuffer, std::ios::binary);
	}
	else {
		std::string absPath = path.rfind("res://", 0) == 0
			? FileManager::getInstance().VirtualToAbsolute(path).string()
			: path;

		auto fileStream = std::make_unique<std::ifstream>(absPath, std::ios::binary);
		if (!fileStream->is_open()) {
			Console::PrintError("ParseTopLevelSceneObjects: failed to open file for reading {}").Format(absPath);
			return false;
		}
		in = std::move(fileStream);
	}

	BinaryReader r(*in);

	uint32_t magic = r.Read<uint32_t>();
	if (magic != sceneMagicByte) {
		Console::PrintError("ParseTopLevelSceneObjects: file is not a valid .fscene file");
		return false;
	}

	uint32_t ver = r.Read<uint32_t>();
	if (ver != sceneVersion) {
		Console::PrintError("ParseTopLevelSceneObjects: unsupported .fscene file version {}").Format((int)ver);
		return false;
	}

	uint32_t objectCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < objectCount; i++) {
		auto obj = std::make_unique<Object>();
		obj->Deserialize(r);
		outObjects.push_back(std::move(obj));
	}

	std::unordered_map<uint64_t, Object*> objectsById;
	for (auto& obj : outObjects)
		objectsById[obj->id] = obj.get();

	for (auto& obj : outObjects) {
		if (obj->parentID != -1 && objectsById.count(static_cast<uint64_t>(obj->parentID))) {
			obj->SetParent(objectsById[static_cast<uint64_t>(obj->parentID)]);
		}
		else {
			obj->parent = nullptr;
			obj->parentID = -1;
		}
	}

	std::vector<Constraint*> savedLiveConstraints;
	std::swap(savedLiveConstraints, PhysicsEngine::getInstance().registeredPGSConstraints);

	uint32_t constraintCount = r.Read<uint32_t>();
	for (uint32_t i = 0; i < constraintCount; i++) {
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
	}

	outConstraints = PhysicsEngine::getInstance().registeredPGSConstraints;
	PhysicsEngine::getInstance().registeredPGSConstraints = std::move(savedLiveConstraints);

	outObjectCount = objectCount;
	return true;
}

bool SceneManager::ExpandSceneRoot(Object* root, std::vector<std::unique_ptr<Object>>& ownerObjects,
	std::vector<Constraint*>& ownerConstraints, bool activateNow, std::vector<std::string>& expansionStack) {

	if (!root || !root->isSceneRoot || root->sourceScenePath.empty()) return true;

	if (!EngineManager::getInstance().isPlayer && root->sourceScenePath.rfind("res://", 0) != 0) {
		std::string normalized = FileManager::getInstance().AbsoluteToVirtual(root->sourceScenePath);
		if (!normalized.empty() && normalized != root->sourceScenePath) {
			root->sourceScenePath = normalized;
			MarkActiveDirty();
		}
	}

	for (auto& p : expansionStack) {
		if (p == root->sourceScenePath) {
			Console::PrintError("ExpandSceneRoot: cyclic scene reference detected, skipping {}").Format(root->sourceScenePath);
			return false;
		}
	}
	expansionStack.push_back(root->sourceScenePath);

	std::vector<std::unique_ptr<Object>> loadedObjects;
	std::vector<Constraint*> loadedConstraints;
	uint32_t objectCount = 0;
	if (!ParseTopLevelSceneObjects(root->sourceScenePath, loadedObjects, loadedConstraints, objectCount)) {
		Console::PrintError("ExpandSceneRoot: failed to load {}").Format(root->sourceScenePath);
		expansionStack.pop_back();
		return false;
	}

	for (auto& obj : loadedObjects) {
		if (obj->parent == nullptr) {
			obj->SetParent(root);
		}
		obj->hideInHierarchy = true;
		if (auto* mic = obj->GetComponent<MouseInteractComponent>()) {
			mic->SetEnabled(false);
		}
	}

	for (auto& obj : loadedObjects)
		for (auto& c : obj->components)
			c->PostLoad();

	std::vector<Object*> nestedRoots;
	for (auto& obj : loadedObjects) {
		if (obj->isSceneRoot) nestedRoots.push_back(obj.get());
	}
	for (Object* nestedRoot : nestedRoots) {
		ExpandSceneRoot(nestedRoot, loadedObjects, loadedConstraints, activateNow, expansionStack);
	}

	if (TransformComponent* rootTransform = root->GetComponent<TransformComponent>()) {
		glm::vec3 rootWorldPos = rootTransform->GetWorldPosition();
		for (auto& obj : loadedObjects) {
			if (obj->parent != root) continue;
			TransformComponent* childTransform = obj->GetComponent<TransformComponent>();
			if (!childTransform) continue;
			childTransform->UpdateWorldPosition(childTransform->GetWorldPosition() + rootWorldPos);
		}
	}

	if (activateNow) {
		for (auto& obj : loadedObjects)
			for (auto& c : obj->components)
				c->Activate();
	}

	for (Constraint* c : loadedConstraints) {
		ownerConstraints.push_back(c);
	}

	for (auto& obj : loadedObjects) {
		ownerObjects.push_back(std::move(obj));
	}

	expansionStack.pop_back();
	return true;
}

void SceneManager::RefreshSceneInstances(std::vector<std::unique_ptr<Object>>& objects, std::vector<Constraint*>& constraints) {
	std::vector<Object*> outermostRoots;
	for (auto& obj : objects) {
		if (!obj->isSceneRoot) continue;

		bool nested = false;
		for (Object* p = obj->parent; p != nullptr; p = p->parent) {
			if (p->isSceneRoot) { nested = true; break; }
		}
		if (!nested) outermostRoots.push_back(obj.get());
	}

	for (Object* root : outermostRoots) {
		RefreshSceneRootInstance(root, objects, constraints);
	}
}

void SceneManager::RefreshSceneRootInstance(Object* root, std::vector<std::unique_ptr<Object>>& objects, std::vector<Constraint*>& constraints) {
	if (!root || !root->isSceneRoot || root->sourceScenePath.empty()) return;

	std::vector<Object*> oldSubtree;
	std::function<void(Object*)> collect = [&](Object* o) {
		for (Object* child : o->children) {
			if (child->hideInHierarchy) {
				oldSubtree.push_back(child);
				collect(child);
			}
		}
		};
	collect(root);

	if (oldSubtree.empty()) {
		std::vector<std::string> expansionStack;
		ExpandSceneRoot(root, objects, constraints, false, expansionStack);
		return;
	}

	std::unordered_set<Object*> oldSet(oldSubtree.begin(), oldSubtree.end());

	constraints.erase(std::remove_if(constraints.begin(), constraints.end(),
		[&](Constraint* c) {
			return (c->objectA.obj && oldSet.count(c->objectA.obj)) ||
				(c->objectB.obj && oldSet.count(c->objectB.obj));
		}), constraints.end());

	if (Object* sel = EditorManager::getInstance().selectedObject; sel && oldSet.count(sel)) {
		EditorManager::getInstance().SetSelectedObject(nullptr);
	}

	root->children.erase(std::remove_if(root->children.begin(), root->children.end(),
		[&](Object* c) { return oldSet.count(c) != 0; }), root->children.end());

	for (Object* o : oldSubtree) {
		o->OnDelete();
	}

	objects.erase(std::remove_if(objects.begin(), objects.end(),
		[&](std::unique_ptr<Object>& o) { return oldSet.count(o.get()) != 0; }), objects.end());

	std::vector<std::string> expansionStack;
	ExpandSceneRoot(root, objects, constraints, false, expansionStack);
}

void SceneManager::RequestLoadScene(const std::string& path) {
	pendingSceneLoadPath = path;
	hasPendingSceneLoad = true;
}

void SceneManager::ProcessPendingSceneLoad() {
	if (!hasPendingSceneLoad) return;
	hasPendingSceneLoad = false;

	std::string path = std::move(pendingSceneLoadPath);
	pendingSceneLoadPath.clear();

	LoadSceneFromFile(path);
	ScriptManager::getInstance().RunAllScriptsLoad();
	ScriptManager::getInstance().RunAllScriptsStart();
	EngineManager::getInstance().SwitchPhysicsMode(EngineManager::PhysicsMode::Simulate);
}

void SceneManager::DeactivateLiveScene() {
	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& c : obj->components) {
			c->Deactivate();
		}
	}
}

void SceneManager::MarkActiveDirty() {
	if (activeIndex >= 0) openScenes[activeIndex].isDirty = true;
}

bool SceneManager::AnySceneDirty() const {
	for (const OpenScene& scene : openScenes) {
		if (scene.isDirty) return true;
	}
	return false;
}

const std::string& SceneManager::GetCurrentSceneFile() const {
	static const std::string empty;
	return activeIndex >= 0 ? openScenes[activeIndex].filePath : empty;
}