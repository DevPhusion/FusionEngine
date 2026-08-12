#include "../../Header Files/Core/SceneManager.h"
#include "../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraints.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"

namespace fs = std::filesystem;

SceneManager::SceneManager() {
	openScenes.push_back(OpenScene{});
	activeIndex = 0;
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

	ActivateLiveScene();
}

int SceneManager::FindSceneByPath(const std::string& path) const {
	if (path.empty()) return -1;
	for (int i = 0; i < (int)openScenes.size(); i++) {
		if (openScenes[i].filePath == path) return i;
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

	openScenes.push_back(OpenScene{});
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

	openScenes.erase(openScenes.begin() + index);

	if (openScenes.empty()) {
		openScenes.push_back(OpenScene{});
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
		if (obj->hideInHierarchy) continue;
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
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		Console::PrintError("LoadScene: failed to open file for reading {}").Format(path);
		return;
	}

	BinaryReader r(in);

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

	ActivateLiveScene();

	OpenScene& active = openScenes[activeIndex];
	active.filePath = path;
	active.displayName = fs::path(path).filename().string();
	active.isDirty = false;
}

void SceneManager::NewScene() {
	ClearLiveScene();

	OpenScene& active = openScenes[activeIndex];
	active.filePath.clear();
	active.displayName = "New Scene.fscene";
	active.isDirty = false;
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