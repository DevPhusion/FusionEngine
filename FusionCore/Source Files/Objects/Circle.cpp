#include "../../Header Files/Objects/Circle.h"

Circle::Circle(Shader shader, std::string texture_path) : Object(shader) {
	AddComponent(std::make_unique<RenderComponent>(this, std::vector<float> {}, shader, texture_path));
	auto* render = GetComponent<RenderComponent>();
	CircleShape shape = CircleShape();
	AddComponent(std::make_unique<TransformComponent>(this, shader, render->GetCenter()));
	shape.center = GetComponent<TransformComponent>()->GetWorldPosition();
	shape.radius = 1.0f;
	render->SetShape(shape);
	AddComponent(std::make_unique<MouseInteractComponent>(this, true));
	AddComponent(std::make_unique<CollisionComponent>(this));
	AddComponent(std::make_unique<RigidBodyComponent>(this));
	AddComponent(std::make_unique<ConstraintComponent>(this));
}


void Circle::Process(float delta) {

}
