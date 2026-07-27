#include "../../Header Files/Core/ObjectManager.h"

void ObjectManager::ProcessObjects(float delta) {
	for (int i = 0; i < allObjects.size(); i++)
	{
		allObjects[i]->Process(delta);
	}
}

void ObjectManager::AddObject() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj.get()->shader, ""));
	RectangleShape shape = RectangleShape();
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj.get()->shader, obj.get()->GetComponent<RenderComponent>()->GetCenter()));
	shape.center = obj->GetComponent<TransformComponent>()->GetWorldPosition();
	shape.width = 0.1f;
	shape.height = 0.1f;
	obj->GetComponent<RenderComponent>()->SetShape(shape);
	obj->AddComponent(std::make_unique<MouseInteractComponent>(obj.get(), false));
	obj->AddComponent(std::make_unique<CollisionComponent>(obj.get()));

	allObjects.push_back(std::move(obj));
}

void ObjectManager::AddBox() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Box> box = std::make_unique<Box>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"), "");
	allObjects.push_back(std::move(box));
}

void ObjectManager::AddCircle() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Circle> circle = std::make_unique<Circle>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"), "");
	allObjects.push_back(std::move(circle));
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
	std::unique_ptr<Polygon> poly = std::make_unique<Polygon>(vertices, Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"), "");

	auto* vc = poly->GetComponent<VertexComponent>();
	auto* tc = poly->GetComponent<TransformComponent>();

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
	allObjects.push_back(std::move(poly));
	vertices.clear();
	vertexPoints.clear();
}

void ObjectManager::AddSoftBox() {
	EngineManager::getInstance().EngineChangeEvent();
	std::unique_ptr<Object> obj = std::make_unique<Object>(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	RectangleShape shape = RectangleShape();
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, render->GetCenter()));
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
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	CircleShape shape = CircleShape();
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, render->GetCenter()));
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
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), vertices, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	PolygonShape shape = PolygonShape();
	shape.vertices = vertices;
	render->SetShape(shape);
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, render->GetCenter()));
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
	obj->AddComponent(std::make_unique<RenderComponent>(obj.get(), std::vector<float> {}, obj->shader, ""));
	auto* render = obj->GetComponent<RenderComponent>();
	RectangleShape shape = RectangleShape();
	obj->AddComponent(std::make_unique<TransformComponent>(obj.get(), obj->shader, render->GetCenter()));
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
		pointIndicator->hidden = true;
		vertexPoints.push_back(pointIndicator.get());
		allObjects.push_back(std::move(pointIndicator));
	}
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
		obj->components[i].get()->CopyTo(newObj.get());
	}
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
				allObjects.erase(allObjects.begin() + i);
				for (int j = 0; j < points.size(); j++)
				{
					RemoveObject(points[j]);
				}
			}
			else {
				obj->OnDelete();
				allObjects.erase(allObjects.begin() + i);
			}
		}
	}
}