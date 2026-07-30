#include "../../Header Files/Core/ObjectManager.h"

void ObjectManager::AddObject() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj.get()->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj.get()->shader, obj.get()->GetComponent<EditorRenderComponent>()->GetCenter()));
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), false));

	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddCamera() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj.get()->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj.get()->shader, obj.get()->GetComponent<EditorRenderComponent>()->GetCenter()));
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), false));
	obj->AddComponent(std::make_unique<CameraComponent>(obj.get()));

	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddBox() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
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
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddCircle() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
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
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddPolygon() {
	if (vertexPoints.size() < 3) {
		Console::PrintError("Invalid polygon");
		for (int i = 0; i < vertexPoints.size(); i++)
		{
			RemoveObject(vertexPoints[i]);
		}
		vertexPoints.clear();
		vertices.clear();
		return;
	}

	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	PolygonShape shape = PolygonShape();
	shape.vertices = vertices;
	render->SetShape(shape);
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	obj->GetComponent<TransformComponent>()->UpdateWorldPosition(obj->GetComponent<TransformComponent>()->GetWorldPosition());
	obj->AddComponent(std::make_unique<VertexComponent>(obj.get()));
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<RigidBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));

	auto* vc = obj->GetComponent<VertexComponent>();
	auto* tc = obj->GetComponent<TransformComponent>();

	if (vc) {
		vc->SetVertexPoints(vertexPoints);
	}
	else {
		for (int i = 0; i < vertexPoints.size(); i++)
		{
			vertexPoints[i]->GetComponent<RenderComponent>()->SetEnabled(false);
		}
	}
	tc->SetOriginTransform(Camera::getInstance().viewMatrixInverse);
	allObjects.push_back(std::move(obj));
	vertices.clear();
	vertexPoints.clear();
}

void ObjectManager::AddSoftBox() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
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
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddSoftCircle() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
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
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddSoftPolygon() {
	if (vertexPoints.size() < 3) {
		Console::PrintError("Invalid polygon");
		for (int i = 0; i < vertexPoints.size(); i++)
		{
			RemoveObject(vertexPoints[i]);
		}
		vertexPoints.clear();
		vertices.clear();
		return;
	}

	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	PolygonShape shape = PolygonShape();
	shape.vertices = vertices;
	render->SetShape(shape);
	obj->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());
	obj->GetComponent<TransformComponent>()->UpdateWorldPosition(obj->GetComponent<TransformComponent>()->GetWorldPosition());
	obj->AddComponent(std::make_unique<VertexComponent>(obj.get()));
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), true));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));
	obj->AddComponent(std::make_unique<SoftBodyComponent>(obj.get()));
	obj->AddComponent(std::make_unique<ConstraintComponent>(obj.get()));

	auto* vc = obj->GetComponent<VertexComponent>();
	auto* tc = obj->GetComponent<TransformComponent>();

	if (vc) {
		vc->SetVertexPoints(vertexPoints);
	}
	else {
		for (int i = 0; i < vertexPoints.size(); i++)
		{
			vertexPoints[i]->GetComponent<RenderComponent>()->SetEnabled(false);
		}
	}
	tc->SetOriginTransform(Camera::getInstance().viewMatrixInverse);
	allObjects.push_back(std::move(obj));
	vertices.clear();
	vertexPoints.clear();
}

void ObjectManager::AddFluid() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<EditorRenderComponent>(obj.get(), obj->shader, "Resources/Images/Object.png", 0.075f));
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, glm::vec3(0.0f)));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
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
	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddPolygonVertex() {
	if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::AddVertex) {
		vertices.push_back(InputManager::glX);
		vertices.push_back(InputManager::glY);
		vertices.push_back(0.0f); // Z coordinate
		vertices.push_back(InputManager::glX); // U
		vertices.push_back(InputManager::glY); // V

		std::unique_ptr<VertexPoint> pointIndicator = std::make_unique<VertexPoint>(InputManager::glX, InputManager::glY, Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
		pointIndicator->hideInHierarchy = true;
		vertexPoints.push_back(pointIndicator.get());
		allObjects.push_back(std::move(pointIndicator));
	}
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

VertexPoint* ObjectManager::CopyVertex(VertexPoint* vert) {
	std::unique_ptr<VertexPoint> newVert = std::make_unique<VertexPoint>(vert->x, vert->y, Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	VertexPoint* returnObj = newVert.get();
	newVert->UpdatePosition(vert->x, vert->y);
	allObjects.push_back(std::move(newVert));
	return returnObj;
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

	std::string baseName = obj->name.empty() ? "Object" : obj->name;
	newObj->name = GenerateUniqueName(baseName, nullptr);

	newObj->SetParent(obj->parent);

	Object* returnObj = newObj.get();
	allObjects.push_back(std::move(newObj));
	return returnObj;
}

void ObjectManager::RemoveObject(Object* obj) {
	EngineManager::getInstance().EngineChangeEvent();
	for (int i = 0; i < allObjects.size(); i++)
	{
		if (allObjects[i].get() == obj) {
			if (obj->HasComponent<VertexComponent>()) {
				std::vector<VertexPoint*> points = obj->GetComponent<VertexComponent>()->vertexPoints;
				obj->OnDelete();
				for (auto* obj : allObjects[i]->children)
				{
					obj->SetParent(allObjects[i]->parent);
				}
				allObjects.erase(allObjects.begin() + i);
				for (int j = 0; j < points.size(); j++)
				{
					RemoveObject(points[j]);
				}
			}
			else {
				obj->OnDelete();
				for (auto* obj : allObjects[i]->children)
				{
					obj->SetParent(allObjects[i]->parent);
				}
				allObjects.erase(allObjects.begin() + i);
			}
		}
	}
}