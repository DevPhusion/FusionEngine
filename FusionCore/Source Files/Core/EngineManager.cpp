#include "../../Header Files/Core/EngineManager.h"
#include "../../Header Files/Core/ObjectManager.h"
#include "../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "../../Header Files/Core/Editor/Windows/Viewport.h"

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
		if (obj->hideInHierarchy) continue;

		if (auto* cc = obj->GetComponent<ConstraintComponent>()) {
			for (auto& c : cc->appliedConstraints)
				SavedState.Constraints.push_back(c->Clone());
		}

		SavedState.Objects.push_back(std::move(obj->Clone()));
	}

	for (int i = 0; i < cachedSaveObjects.size(); i++)
		SavedState.Objects.push_back(std::move(cachedSaveObjects[i]));
	cachedSaveObjects.clear();
	if (FileManager::getInstance().currentProjectFile != "") FileManager::getInstance().isSceneSaved = true;
}

void EngineManager::LoadEngineState() {
	EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<Object*> toRemove;
	toRemove.reserve(ObjectManager::getInstance().allObjects.size());
	for (auto& obj : ObjectManager::getInstance().allObjects)
		toRemove.push_back(obj.get());
	for (Object* obj : toRemove)
		ObjectManager::getInstance().RemoveObject(obj);
	Renderer::getInstance().constraintEditGizmos->registeredConstraints.clear();
	ObjectManager::getInstance().allObjects.clear();

	for (int i = 0; i < SavedState.Objects.size(); i++)
	{
		for (int j = 0; j < SavedState.Objects[i]->components.size(); j++)
			SavedState.Objects[i]->components[j]->SetEnabled(SavedState.Objects[i]->components[j]->pendingEnabled);
		ObjectManager::getInstance().allObjects.push_back(std::move(SavedState.Objects[i]));
	}
	SavedState.Objects.clear();

	std::unordered_map<uint64_t, Object*> objectsById;
	for (auto& obj : ObjectManager::getInstance().allObjects)
		objectsById[obj->id] = obj.get();

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		if (obj->parentID != -1 && objectsById.count(obj->parentID)) {
			obj->SetParent(objectsById[obj->parentID]);
		}
		else {
			obj->parent = nullptr;
			obj->parentID = -1; 
		}
	}

	for (auto& constraint : SavedState.Constraints)
	{
		Object* a = objectsById.count(constraint->objectIdA) ? objectsById[constraint->objectIdA] : nullptr;
		Object* b = objectsById.count(constraint->objectIdB) ? objectsById[constraint->objectIdB] : nullptr;
		if (!a) continue; 

		if (!a->HasComponent<ConstraintComponent>())
			a->AddComponent(std::make_unique<ConstraintComponent>(a));
		glm::vec3 attachPointA = constraint->attachPointA;
		bool useACenter = constraint->UseCenterA();
		glm::vec3 attachPointB = constraint->attachPointB;
		bool useBCenter = constraint->UseCenterB();
		constraint->SetObjectA(PhysicsEngine::getInstance().GetBodyFromObject(a));
		constraint->SetObjectB(PhysicsEngine::getInstance().GetBodyFromObject(b));
		Renderer::getInstance().constraintEditGizmos->RegisterConstraint(constraint.get());
		a->GetComponent<ConstraintComponent>()->AddConstraint(constraint);
		constraint->attachPointA = attachPointA;
		constraint->attachPointB = attachPointB;
		constraint->useCenterA = useACenter;
		constraint->useCenterB = useBCenter;
	}
	SavedState.Constraints.clear();

	for (auto& obj : ObjectManager::getInstance().allObjects) {
		for (auto& c : obj->components) {
			c->PostLoad();
		}
	}

	if (FileManager::getInstance().currentProjectFile != "") FileManager::getInstance().isSceneSaved = false;
}

void EngineManager::SceneChangeEvent() {
	if (EnginePhysicsMode == PhysicsMode::Stop) {
		FileManager::getInstance().isSceneSaved = false;
	}
}

void EngineManager::EngineChangeEvent() {
	if (EnginePhysicsMode == PhysicsMode::Stop) {
		FileManager::getInstance().isProjectSaved = false;
	}
}

void EngineManager::SetGameResolution(float width, float height) {
	if (width <= 0.0f || height <= 0.0f) return;

	resolutionWidth = width;
	resolutionHeight = height;
	gameAspectRatio = resolutionWidth / resolutionHeight;

	Viewport* gameViewport = EditorManager::getInstance().gameViewport;
	if (gameViewport) {
		gameViewport->Resize((int)resolutionWidth, (int)resolutionHeight); // NEW - see step 3
	}
}

ViewportRect EngineManager::GetPlayerViewportRect() const {
	ViewportRect rect;
	if (windowWidth <= 0 || windowHeight <= 0 || gameAspectRatio <= 0) {
		rect.width = (int)windowWidth;
		rect.height = (int)windowHeight;
		return rect;
	}

	float windowAspect = windowWidth / windowHeight;

	if (windowAspect > gameAspectRatio) {
		rect.height = (int)windowHeight;
		rect.width = (int)(windowHeight * gameAspectRatio);
	}
	else {
		rect.width = (int)windowWidth;
		rect.height = (int)(windowWidth / gameAspectRatio);
	}

	rect.x = (int)((windowWidth - rect.width) * 0.5f);
	rect.y = (int)((windowHeight - rect.height) * 0.5f);
	return rect;
}

void EngineManager::SerializeEngineSettings(BinaryWriter& w) {
	Settings& s = EngineSettings;

	w.WriteString(EngineSettings.mainScenePath);

	w.Write(resolutionWidth);
	w.Write(resolutionHeight);

	w.Write(s.backgroundColor);
	w.Write(s.drawBackgroundGrid);
	w.Write(s.drawObjectWireframe);
	w.Write(s.drawBroadPhaseBounds);
	w.Write(s.drawCollisionShapes);
	w.Write(s.drawCollisionNormals);
	w.Write(s.drawContactPoints);
	w.Write(s.drawSoftBodyPointMasses);
	w.Write(s.drawSoftBodySprings);
	w.Write(s.drawVirtualSoftBodyProxies);
	w.Write(s.drawFluidsAsParticles);
	w.Write(s.drawFluidsVelocityField);
	w.Write(static_cast<uint32_t>(s.fluidHeatmapMode)); 
}

void EngineManager::DeserializeEngineSettings(BinaryReader& r) {
	Settings& s = EngineSettings;

	EngineSettings.mainScenePath = r.ReadString();

	float resWidth = r.Read<float>();
	float resHeight = r.Read<float>();
	SetGameResolution(resWidth, resHeight); 

	s.backgroundColor = r.Read<glm::vec4>();
	s.drawBackgroundGrid = r.Read<bool>();
	s.drawObjectWireframe = r.Read<bool>();
	s.drawBroadPhaseBounds = r.Read<bool>();
	s.drawCollisionShapes = r.Read<bool>();
	s.drawCollisionNormals = r.Read<bool>();
	s.drawContactPoints = r.Read<bool>();
	s.drawSoftBodyPointMasses = r.Read<bool>();
	s.drawSoftBodySprings = r.Read<bool>();
	s.drawVirtualSoftBodyProxies = r.Read<bool>();
	s.drawFluidsAsParticles = r.Read<bool>();
	s.drawFluidsVelocityField = r.Read<bool>();
	s.fluidHeatmapMode = static_cast<FluidHeatmapMode>(r.Read<uint32_t>());
}

void EngineManager::SwitchInteractMode(InteractMode mode) {
	EngineInteractMode = mode;

	for (const auto& [id, func] : InteractModeChangedEvents) {
		func();
	}
}

void EngineManager::SwitchPhysicsMode(PhysicsMode mode) {
	EnginePrevPhysicsMode = EnginePhysicsMode;
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
	FileManager& FM = FileManager::getInstance();
	EngineManager& eng = EngineManager::getInstance();
	if (!ProjectLauncher::getInstance().HasEnteredProject()) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}
	if (!FM.isSceneSaved || !FM.isProjectSaved) {
		glfwSetWindowShouldClose(window, GLFW_FALSE);
		eng.pendingClose = true;
	}
}