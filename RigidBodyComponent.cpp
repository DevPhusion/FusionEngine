#include "RigidBodyComponent.h"
#include "PhysicsEngine.h"
#include "MouseInteractComponent.h"

RigidBodyComponent::RigidBodyComponent(Object* parent) : ComponentBase<RigidBodyComponent>(parent) {
	Name = "Rigid Body Component";
	this->inverseMass = 1;
	this->acceleration = glm::vec3(0.0f, -9.8f, 0.0f);
	this->velocity = glm::vec3(0);
	this->netForce = glm::vec3(0);
	CalculateInertia();

	MouseInteractComponent* mc = parent->GetComponent<MouseInteractComponent>();
	if (mc) mc->physicsInteract = true;
}

void RigidBodyComponent::CopyTo(Object* other) {
	RigidBodyComponent* target = other->GetComponent<RigidBodyComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<RigidBodyComponent>(other));
		target = other->GetComponent<RigidBodyComponent>();
	}

	target->netForceDisplay = netForceDisplay;
	target->netAcceleration = netAcceleration;
	target->torqueDisplay = torqueDisplay;
	target->angularAcceleration = angularAcceleration;
	target->velocity = velocity;
	target->acceleration = acceleration;
	target->netForce = netForce;
	target->angularVelocity = angularVelocity;
	target->Torque = Torque;
	target->Inertia = Inertia;
	target->inverseInertia = inverseInertia;
	target->inverseMass = inverseMass;
}

std::unique_ptr<Component> RigidBodyComponent::Clone(Object* parent) {
	std::unique_ptr<RigidBodyComponent> comp = std::make_unique<RigidBodyComponent>(parent);
	comp->velocity = velocity;
	comp->angularVelocity = angularVelocity;
	comp->Inertia = Inertia;
	comp->inverseInertia = inverseInertia;
	comp->inverseMass = inverseMass;
	comp->SetEnabled(false);
	return comp;
}

void RigidBodyComponent::ProcessInspectorUI() {
	if (!this->parent->GetComponent<TransformComponent>()->Enabled) {
		SetEnabled(false);
	}

	ImGui::Text("Mass ");
	ImGui::SameLine();
	float mass = 1 / inverseMass;
	if (ImGui::InputFloat("##Mass", &mass, 0.0f, 0.0f, "%.3f kg")) {
		if (mass <= 0) {
			std::cout << "Mass can't be 0 or lower" << std::endl;
		}
		else {
			inverseMass = 1 / mass;
			CalculateInertia();
		}
	}

	ImGui::Text("Velocity ");
	ImGui::SameLine();
	float velocity[] = { this->velocity.x, this->velocity.y };
	if (ImGui::InputFloat2("##Velocity", velocity, "%.3f m/s")) {
		this->velocity = glm::vec3(velocity[0], velocity[1], 0);
	}

	ImGui::Text("Acceleration ");
	ImGui::SameLine();
	float accel[] = { this->netAcceleration.x, this->netAcceleration.y };
	ImGui::InputFloat2("##Acceleration", accel, "%.3f m/s²", ImGuiInputTextFlags_ReadOnly);

	ImGui::Text("Angular Velocity ");
	ImGui::SameLine();
	ImGui::InputFloat("## Angular Velocity", &angularVelocity, 0.0f, 0.0f, "%.3f rad/s");

	ImGui::Text("Angular Acceleration ");
	ImGui::SameLine();
	ImGui::InputFloat("##Angular Acceleration", &angularAcceleration, 0.0f, 0.0f, "%.3f rad/s²", ImGuiInputTextFlags_ReadOnly);

	ImGui::Text("Inertia ");
	ImGui::SameLine();
	ImGui::InputFloat("## Inertia", &Inertia, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::Text("Torque ");
	ImGui::SameLine();
	ImGui::InputFloat("## Torque", &torqueDisplay, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::Text("Net Force ");
	ImGui::SameLine();
	float force[] = { this->netForceDisplay.x, this->netForceDisplay.y };
	if (ImGui::InputFloat2("##Net Force", force, "%.3f N")) {
		this->netForce = glm::vec3(force[0], force[1], 0);
	}

	ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (ImGui::TreeNodeEx("Forces", root_flags)) {
		for (int i = 0; i < forceDisplayFunc.size(); i++)
		{
			(*forceDisplayFunc[i])(i);
		}

		ImGui::TreePop();
	}
}

void RigidBodyComponent::OnDelete() {
	PhysicsEngine::getInstance().UnRegisterAllForce(parent);
	forceDisplayFunc.clear();
	MouseInteractComponent* mc = parent->GetComponent<MouseInteractComponent>();
	if (mc) mc->physicsInteract = false;
}

void RigidBodyComponent::IntegrateVelocities(float delta) {
	if (!Enabled) {
		return;
	}

	glm::vec3 resultingAcc = acceleration;
	resultingAcc += netForce * inverseMass;

	float angularAcc = Torque * inverseInertia;

	velocity += resultingAcc * delta;
	angularVelocity += angularAcc * delta;

	velocity *= powf(linearDamping, delta);
	angularVelocity *= powf(angularDamping, delta);

	if (glm::length(velocity) < 0.001f) velocity = glm::vec3(0.0f);
	if (std::abs(angularVelocity) < 0.001f) angularVelocity = 0.0f;

	netAcceleration = glm::vec2(resultingAcc.x, resultingAcc.y);
	netForceDisplay = glm::vec2(netForce.x, netForce.y);
	torqueDisplay = Torque;
	angularAcceleration = angularAcc;

	ClearAccumulators();
}

void RigidBodyComponent::IntegratePositions(float delta) {
	if (!Enabled) {
		return;
	}

	TransformComponent* transform = this->parent->GetComponent<TransformComponent>();
	glm::vec3 position = transform->GetWorldPosition();
	float rotation = transform->rotation;

	position += velocity * delta;
	rotation += angularVelocity * delta;
	rotation = atan2(sin(rotation), cos(rotation));

	transform->rotation = rotation;
	transform->UpdateWorldPosition(position);
}

void RigidBodyComponent::ClearAccumulators() {
	netForce = glm::vec3(0);
	Torque = 0.0f;
}

void RigidBodyComponent::AddForce(glm::vec3 force) {
	netForce += force;
}

void RigidBodyComponent::AddForceAtBodyPoint(glm::vec3 force, glm::vec3 point) {
	glm::vec3 worldPoint = this->parent->GetComponent<TransformComponent>()->ProjectToWorld(point);
	AddForceAtPoint(force, worldPoint);
}

void RigidBodyComponent::AddForceAtPoint(glm::vec3 force, glm::vec3 point) {
	glm::vec3 relativePoint = point - parent->GetComponent<TransformComponent>()->GetWorldPosition();
	Torque += relativePoint.x * force.y - relativePoint.y * force.x;
	netForce += force;
}

float calcTriangleArea(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
	return 0.5f * std::abs((a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y)));
}

float calculateTriangleInertia(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 centerOfMass, float massTriangle) {
	float inertia = (massTriangle / 36) * (glm::length2(a - b) + glm::length2(b - c) + glm::length2(c - a));
	glm::vec3 centroid = (a + b + c) / 3.0f;

	float distSquared = glm::length2(centroid - centerOfMass);
	return inertia + (massTriangle * distSquared);
}

void RigidBodyComponent::CalculateInertia() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	std::vector<std::vector<float>> points = rc->points;
	std::vector<unsigned int> indices = rc->Indices;
	
	float sum = 0;
	float mass = 1.0f / inverseMass;

	for (int i = 0; i < indices.size(); i+=3)
	{
		glm::vec3 a = glm::vec3(points[indices[i]][0], points[indices[i]][1], 0.0f);
		glm::vec3 b = glm::vec3(points[indices[i+1]][0], points[indices[i+1]][1], 0.0f);
		glm::vec3 c = glm::vec3(points[indices[i+2]][0], points[indices[i+2]][1], 0.0f);

		float m_triangle = mass * (calcTriangleArea(a, b, c) / rc->GetArea());
		sum += calculateTriangleInertia(tc->ProjectToWorld(a), tc->ProjectToWorld(b), tc->ProjectToWorld(c), tc->GetWorldPosition(), m_triangle);
	}

	this->Inertia = sum;
	if (Inertia > 0) {
		this->inverseInertia = 1.0f / Inertia;
	}
}

void RigidBodyComponent::AddDisplayFunc(std::shared_ptr<std::function<void(int)>> func) {
	forceDisplayFunc.push_back(func);
}

void RigidBodyComponent::RemoveDisplayFunc(std::shared_ptr<std::function<void(int)>> func) {
	for (int i = 0; i < forceDisplayFunc.size(); i++)
	{
		if (forceDisplayFunc[i] == func) {
			forceDisplayFunc.erase(forceDisplayFunc.begin() + i);
		}
	}
}