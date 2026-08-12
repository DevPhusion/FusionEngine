#include "../../Header Files/Core/ObjectManager.h"

void ObjectManager::AddObject(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj.get()->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj.get()->shader, obj.get()->GetComponent<EditorRenderComponent>()->GetCenter()));
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), false));
	for (auto& c : obj->components) {
		c->Activate();
	}
	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddCamera(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj.get()->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj.get()->shader, obj.get()->GetComponent<EditorRenderComponent>()->GetCenter()));
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), false));
	obj->AddComponent(std::make_unique<CameraComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddBox(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float>{}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	RectangleShape shape = RectangleShape();
	shape.center = obj->GetComponent<TransformComponent>()->GetWorldPosition();
	shape.width = 1.0f;
	shape.height = 1.0f;
	render->SetShape(shape);
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<RigidBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddCircle(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	CircleShape shape = CircleShape();
	shape.center = obj->GetComponent<TransformComponent>()->GetWorldPosition();
	shape.radius = 1.0f;
	render->SetShape(shape);
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<RigidBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddPolygon(Object* parent) {
	const std::vector<glm::vec3>& worldVerts = Renderer::getInstance().polygonEditGizmos->GetLocalVertices();

	if (worldVerts.size() < 3) {
		Console::PrintError("Invalid polygon");
		Renderer::getInstance().polygonEditGizmos->EndEdit();
		return;
	}

	glm::vec3 centroid(0.0f);
	for (auto& v : worldVerts) centroid += v;
	centroid /= (float)worldVerts.size();

	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));

	TransformComponent* tc = obj->GetComponent<TransformComponent>();
	tc->UpdateWorldPosition(centroid);

	std::vector<glm::vec3> localVerts;
	localVerts.reserve(worldVerts.size());
	for (auto& v : worldVerts) {
		localVerts.push_back(tc->ProjectToWorld(v, true));
	}

	std::vector<float> vertexData = BuildInterleavedVertices(localVerts);

	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertexData, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	PolygonShape shape;
	shape.vertices = vertexData;
	render->SetShape(shape);

	tc->SetRotationCenter(render->GetCenter());
	tc->worldMatrixDirty = true;

	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<RigidBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));

	Renderer::getInstance().polygonEditGizmos->EndEdit();
}

void ObjectManager::AddSoftBox(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	RectangleShape shape = RectangleShape();
	shape.center = obj->GetComponent<TransformComponent>()->GetWorldPosition();
	shape.width = 1.0f;
	shape.height = 1.0f;
	render->SetShape(shape);
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<SoftBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddSoftCircle(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	CircleShape shape = CircleShape();
	shape.center = obj->GetComponent<TransformComponent>()->GetWorldPosition();
	shape.radius = 1.0f;
	render->SetShape(shape);
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<SoftBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddSoftPolygon(Object* parent) {
	const std::vector<glm::vec3>& worldVerts = Renderer::getInstance().polygonEditGizmos->GetLocalVertices();

	if (worldVerts.size() < 3) {
		Console::PrintError("Invalid polygon");
		Renderer::getInstance().polygonEditGizmos->EndEdit();
		return;
	}

	glm::vec3 centroid(0.0f);
	for (auto& v : worldVerts) centroid += v;
	centroid /= (float)worldVerts.size();

	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));

	TransformComponent* tc = obj->GetComponent<TransformComponent>();
	tc->UpdateWorldPosition(centroid);

	std::vector<glm::vec3> localVerts;
	localVerts.reserve(worldVerts.size());
	for (auto& v : worldVerts) {
		localVerts.push_back(tc->ProjectToWorld(v, true));
	}

	std::vector<float> vertexData = BuildInterleavedVertices(localVerts);

	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertexData, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	PolygonShape shape;
	shape.vertices = vertexData;
	render->SetShape(shape);

	tc->SetRotationCenter(render->GetCenter());
	tc->worldMatrixDirty = true;

	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<SoftBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));

	Renderer::getInstance().polygonEditGizmos->EndEdit();
}

void ObjectManager::AddFluid(Object* parent) {
	EngineManager::getInstance().SceneChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	RectangleShape shape = RectangleShape();
	shape.center = obj->GetComponent<TransformComponent>()->GetWorldPosition();
	shape.width = 10.0f;
	shape.height = 10.0f;
	render->SetShape(shape);
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<FluidComponent>(obj.get()));
	FluidComponent* fc = obj->GetComponent<FluidComponent>();
	for (auto& c : obj->components) {
		c->Activate();
	}

	obj->addedToScene = true;
	obj->SetParent(parent);
	EditorManager::getInstance().RegisterObjectCreated(obj.get());
	allObjects.push_back(std::move(obj));
}

Object* ObjectManager::AddExistingObject(std::unique_ptr<Object> obj, Object* parent) {
	if (!obj) return nullptr;

	EngineManager::getInstance().SceneChangeEvent();

	Object* raw = obj.get();
	raw->name = GenerateUniqueName(raw->name.empty() ? "Object" : raw->name, nullptr);

	if (parent) {
		raw->SetParent(parent);
	}

	raw->addedToScene = true;

	pendingObjects.push_back(std::move(obj));
	return raw;
}

void ObjectManager::FlushPendingObjects() {
	if (pendingObjects.empty()) return;

	for (auto& obj : pendingObjects) {
		for (auto& c : obj->components) {
			c->Activate();
		}
		allObjects.push_back(std::move(obj));
	}
	pendingObjects.clear();
}

std::string ObjectManager::GenerateUniqueName(const std::string& baseName, Object* exclude) {
	std::string candidate = baseName;
	int suffix = 1;
	bool exists = true;

	while (exists) {
		exists = false;
		for (auto& o : allObjects) {
			if (o && o.get() != exclude && o->name == candidate) {
				exists = true;
				break;
			}
		}

		if (exists) {
			candidate = baseName + " (" + std::to_string(suffix) + ")";
			suffix++;
		}
	}

	return candidate;
}

Object* ObjectManager::FindObjectById(uint64_t id) {
	for (auto& obj : allObjects) {
		if (obj && obj->id == id) return obj.get();
	}
	return nullptr;
}

Object* ObjectManager::CopyObject(Object* obj) {
	std::unique_ptr<Object> newObj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

	for (int i = 0; i < obj->components.size(); i++)
	{
		obj->components[i]->CopyTo(newObj.get());
	}

	for (int i = 0; i < newObj->components.size(); i++)
	{
		newObj->components[i]->PostLoad();
	}

	for (auto& c : newObj->components) {
		c->Activate();
	}

	std::string baseName = obj->name.empty() ? "Object" : obj->name;
	newObj->name = GenerateUniqueName(baseName, nullptr);

	newObj->SetParent(obj->parent);

	Object* returnObj = newObj.get();
	newObj->addedToScene = true;
	allObjects.push_back(std::move(newObj));
	return returnObj;
}

void ObjectManager::RemoveObject(Object* obj) {
	bool tracking = !FileManager::getInstance().IsRestoring();
	std::vector<uint64_t> subtreeIds;
	std::vector<uint8_t> before;
	if (tracking) {
		before = FileManager::getInstance().SnapshotObjects({ obj });
		std::function<void(Object*)> collect = [&](Object* o) {
			subtreeIds.push_back(o->id);
			for (auto* c : o->children) collect(c);
			};
		collect(obj);
	}

	EngineManager::getInstance().SceneChangeEvent();
	if (EditorManager::getInstance().selectedObject == obj) EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<Object*> reparentedChildren;
	for (int i = 0; i < allObjects.size(); i++) {
		if (allObjects[i].get() == obj) {
			obj->OnDelete();
			for (auto* child : allObjects[i]->children) {
				child->SetParent(allObjects[i]->parent);
				reparentedChildren.push_back(child);
			}
			allObjects.erase(allObjects.begin() + i);
			break;
		}
	}

	if (tracking) EditorManager::getInstance().RegisterObjectDeleted(before, subtreeIds, reparentedChildren);
}

void ObjectManager::RemoveObjectById(uint64_t id) {
	for (size_t i = 0; i < allObjects.size(); i++) {
		if (allObjects[i]->id == id) {
			if (EditorManager::getInstance().selectedObject == allObjects[i].get())
				EditorManager::getInstance().SetSelectedObject(nullptr);
			allObjects[i]->OnDelete();
			allObjects.erase(allObjects.begin() + i);
			return;
		}
	}
}

void ObjectManager::QueueRemoveObject(Object* obj) {
	if (!obj) return;

	for (Object* o : pendingRemovals) {
		if (o == obj) return; 
	}
	pendingRemovals.push_back(obj);
}

void ObjectManager::FlushPendingRemovals() {
	if (pendingRemovals.empty()) return;

	std::vector<Object*> toRemove = std::move(pendingRemovals);
	pendingRemovals.clear();

	for (Object* obj : toRemove) {
		RemoveObject(obj); 
	}
}

std::vector<float> ObjectManager::BuildInterleavedVertices(const std::vector<glm::vec3>& localVerts) {
	glm::vec3 bmin(INFINITY), bmax(-INFINITY);
	for (auto& v : localVerts) { bmin = glm::min(bmin, v); bmax = glm::max(bmax, v); }
	glm::vec3 range = glm::max(bmax - bmin, glm::vec3(1e-6f));

	std::vector<float> out;
	out.reserve(localVerts.size() * 5);
	for (auto& v : localVerts) {
		float u = (v.x - bmin.x) / range.x;
		float uvY = (v.y - bmin.y) / range.y;
		out.insert(out.end(), { v.x, v.y, 0.0f, u, uvY });
	}
	return out;
}