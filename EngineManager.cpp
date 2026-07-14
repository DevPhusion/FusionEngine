#include "EngineManager.h"
#include "ObjectManager.h"
#include "Constraint.h"
#include "Constraints.h"

void EngineManager::Setup(GLFWwindow* window) {
	int windowWidth, windowHeight;
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
	this->windowWidth = (float)windowWidth;
	this->windowHeight = (float)windowHeight;
	this->aspectRatio = this->windowWidth / this->windowHeight;
	frameCount = 0;

	this->Window = window;                                 
	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
}

void EngineManager::ProcessEngine(float delta) {
	frameCount++;
	time += delta;

	if (time >= 1) {
		fps = frameCount / time;
		frameCount = 0;
		time = 0;
	}
}

void EngineManager::SaveEngineState() {
	SavedState.Objects.clear();
	SavedState.Constraints.clear();

	for (int i = 0; i < ObjectManager::getInstance().allObjects.size(); i++)
	{
		Object* obj = ObjectManager::getInstance().allObjects[i].get();
		if (obj->hidden) continue;

		if (auto* cc = obj->GetComponent<ConstraintComponent>()) {
			for (auto& c : cc->appliedConstraints)
				SavedState.Constraints.push_back(c->Clone());
		}

		SavedState.Objects.push_back(std::move(obj->Clone()));
	}

	for (int i = 0; i < cachedSaveObjects.size(); i++)
		SavedState.Objects.push_back(std::move(cachedSaveObjects[i]));
	cachedSaveObjects.clear();
	if (currentProjectFile != "") isSaved = true;
}

void EngineManager::LoadEngineState() {
	EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<Object*> toRemove;
	toRemove.reserve(ObjectManager::getInstance().allObjects.size());
	for (auto& obj : ObjectManager::getInstance().allObjects)
		toRemove.push_back(obj.get());
	for (Object* obj : toRemove)
		ObjectManager::getInstance().RemoveObject(obj);
	ObjectManager::getInstance().allObjects.clear();

	for (int i = 0; i < SavedState.Objects.size(); i++)
	{
		for (int j = 0; j < SavedState.Objects[i]->components.size(); j++)
			SavedState.Objects[i]->components[j]->SetEnabled(true);
		ObjectManager::getInstance().allObjects.push_back(std::move(SavedState.Objects[i]));
	}
	SavedState.Objects.clear();

	std::unordered_map<uint64_t, Object*> objectsById;
	for (auto& obj : ObjectManager::getInstance().allObjects)
		objectsById[obj->id] = obj.get();

	for (auto& constraint : SavedState.Constraints)
	{
		Object* a = objectsById.count(constraint->objectIdA) ? objectsById[constraint->objectIdA] : nullptr;
		Object* b = objectsById.count(constraint->objectIdB) ? objectsById[constraint->objectIdB] : nullptr;
		if (!a) continue; 

		if (!a->HasComponent<ConstraintComponent>())
			a->AddComponent(std::make_unique<ConstraintComponent>(a));
		constraint->SetObjectA(PhysicsEngine::getInstance().GetBodyFromObject(a));
		constraint->SetObjectB(PhysicsEngine::getInstance().GetBodyFromObject(b));
		constraint->constraintDisplay = constraint->CreateConstraintDisplay();
		a->GetComponent<ConstraintComponent>()->AddConstraint(constraint);
		constraint->ProcessConstraintDisplay();


	}
	SavedState.Constraints.clear();

	if (currentProjectFile != "") isSaved = true;
}

void EngineManager::SaveProjectToFile(const std::string& path) {
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

void EngineManager::LoadProjectFromFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open())
		throw std::runtime_error("LoadProjectFromFile: failed to open file for reading: " + path);

	BinaryReader r(in);

	uint32_t magic = r.Read<uint32_t>();
	if (magic != magicByte)
		throw std::runtime_error("LoadProjectFromFile: file is not a valid .fusion file: " + path);

	uint32_t ver = r.Read<uint32_t>();
	if (ver != version)
		throw std::runtime_error("LoadProjectFromFile: unsupported .fusion file version " + std::to_string(version));

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

	isSaved = true;
}

void EngineManager::NewProject() {
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

void EngineManager::EngineChangeEvent() {
	if (EnginePhysicsMode == PhysicsMode::Stop) {
		isSaved = false;
	}
}

void EngineManager::SwitchInteractMode(InteractMode mode) {
	EngineInteractMode = mode;

	for (const auto& [id, func] : InteractModeChangedEvents) {
		func();
	}
}

void EngineManager::SwitchPhysicsMode(PhysicsMode mode) {
	EnginePhysicsMode = mode;

	for (const auto& [id, func] : PhysicsModeChangedEvents) {
		func();
	}
}

int EngineManager::AddInteractModeChangedEvent(std::function<void()> func) {
	CurrentInteractModeChangedID += 1;
	InteractModeChangedEvents[CurrentInteractModeChangedID] = func;
	return CurrentInteractModeChangedID;
}

int EngineManager::AddPhysicsModeChangedEvent(std::function<void()> func) {
	CurrentPhysicsModeChangedID += 1;
	PhysicsModeChangedEvents[CurrentPhysicsModeChangedID] = func;
	return CurrentPhysicsModeChangedID;
}

void EngineManager::RemoveInteractModeChangedEvent(int ID) {
	InteractModeChangedEvents.erase(ID);
}

void EngineManager::RemovePhysicsModeChangedEvent(int ID) {
	PhysicsModeChangedEvents.erase(ID);
}

void EngineManager::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
	if (width == 0 || height == 0) return; 

	EngineManager& eng = EngineManager::getInstance();
	eng.windowWidth = (float)width;
	eng.windowHeight = (float)height;
	eng.aspectRatio = eng.windowWidth / eng.windowHeight;

	glViewport(0, 0, width, height);
}

void EngineManager::WindowCloseCallback(GLFWwindow* window) {
	EngineManager& eng = EngineManager::getInstance();
	if (!eng.isSaved) {
		glfwSetWindowShouldClose(window, GLFW_FALSE);
		eng.pendingAction = PendingAction::Close;
	}
}