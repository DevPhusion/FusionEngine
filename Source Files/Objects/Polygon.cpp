#include "../../Header Files/Objects/Polygon.h"

Polygon::Polygon(std::vector<float> vertices, Shader shader, std::string texture_path) : Object(shader) {
	AddComponent(std::make_unique<RenderComponent>(this, vertices, shader, texture_path));
	auto* render = GetComponent<RenderComponent>();
	PolygonShape shape = PolygonShape();
	shape.vertices = vertices;
	render->SetShape(shape);
	AddComponent(std::make_unique<TransformComponent>(this, shader, render->GetCenter()));
	AddComponent(std::make_unique<VertexComponent>(this));  
	AddComponent(std::make_unique<MouseInteractComponent>(this, true));
	AddComponent(std::make_unique<CollisionComponent>(this));
	AddComponent(std::make_unique<RigidBodyComponent>(this));
	AddComponent(std::make_unique<ConstraintComponent>(this));
}


void Polygon::Process(float delta) {
	
}