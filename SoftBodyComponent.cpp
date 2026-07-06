#include "SoftBodyComponent.h"
#include "ObjectManager.h"

SoftBodyComponent::SoftBodyComponent(Object* parent) : ComponentBase<SoftBodyComponent>(parent) {
	Name = "Soft Body Component";
	BuildMassAggregate();

	transformCallbackID = parent->GetComponent<TransformComponent>()->AddTransformCallback([this] {UpdateMassAggregate();});

	if (parent->HasComponent<MouseInteractComponent>()) {
		parent->GetComponent<MouseInteractComponent>()->physicsInteract = true;
	}
}

void SoftBodyComponent::ProcessSoftBody(float delta) {
	SyncMeshFromMassAggregate();
}

void SoftBodyComponent::ProcessInspectorUI() {
	ImGui::Text("Mass ");
	ImGui::SameLine();
	float mass = 1.0f / inverseMass;
	if (ImGui::InputFloat("##Mass", &mass, 0.0f, 0.0f, "%.3f kg")) {
		if (mass <= 0.0f) mass = 1 / inverseMass;
		else {
			inverseMass = 1.0f / mass;
			float unitInvMass = MassAggregate.size() / mass;

			for (int i = 0; i < MassAggregate.size(); i++)
			{
				MassAggregate[i]->inverseMass = unitInvMass;
			}
		}
	}

	ImGui::Text("Velocity ");
	ImGui::SameLine();
	float vel[2] = { CenterPM->velocity.x, CenterPM->velocity.y };
	if (ImGui::InputFloat2("##Velocity", vel, "%.3f m/s")) {
		CenterPM->velocity.x = vel[0];
		CenterPM->velocity.y = vel[1];
	}

	ImGui::Text("Acceleration ");
	ImGui::SameLine();
	glm::vec3 finalAccel = CenterPM->baseAcceleration + CenterPM->acceleration;
	float accel[2] = { finalAccel.x, finalAccel.y };
	ImGui::InputFloat2("##Acceleration", accel, "%.3f m/s", ImGuiInputTextFlags_ReadOnly);

	ImGui::Text("Stiffness ");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Stiffness ", &stiffness, 0.0f, 0.0f, "%.3f N/m")) {
		float compliance = (stiffness > 0.0f) ? (1.0f / stiffness) : 0.0f;
		areaConstraint->compliance = compliance;
		for (int i = 0; i < springs.size(); i++)
		{
			springs[i]->compliance = compliance;
		}
	}

	ImGui::Text("Damping ");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Damping ", &damping, 0.0f, 0.0f, "%.3f Ns/m")) {
		for (int i = 0; i < springs.size(); i++)
		{
			springs[i]->damping = damping;
		}
	}

	if (ImGui::Button("Reset shape")) {
		parent->GetComponent<RenderComponent>()->SetShape(parent->GetComponent<RenderComponent>()->currentShape);
		parent->GetComponent<TransformComponent>()->SetRotationCenter(parent->GetComponent<RenderComponent>()->GetCenter());
	}
}

void SoftBodyComponent::CopyTo(Object* other) {
	SoftBodyComponent* target = other->GetComponent<SoftBodyComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<SoftBodyComponent>(other));
		target = other->GetComponent<SoftBodyComponent>();
	}

	target->stiffness = stiffness;
	float compliance = (stiffness > 0.0f) ? (1.0f / stiffness) : 0.0f;
	for (int i = 0; i < target->springs.size(); i++)
	{
		target->springs[i]->compliance = compliance;
	}

	target->damping = damping;
	for (int i = 0; i < target->springs.size(); i++)
	{
		target->springs[i]->damping = damping;
	}

}

void SoftBodyComponent::OnDelete() {
	for (XPBDDistanceConstraint* s : springs)
		PhysicsEngine::getInstance().UnRegisterXPBDConstraint(s);
	springs.clear();
	PhysicsEngine::getInstance().UnRegisterXPBDConstraint(areaConstraint);

	auto& allPms = PhysicsEngine::getInstance().allSoftBodyPointMasses;  
	for (int i = 0; i < MassAggregate.size(); i++)
	{
		PointMass* target = MassAggregate[i].get();
		allPms.erase(std::remove(allPms.begin(), allPms.end(), target), allPms.end());
	}

	MassAggregate.clear();

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc) tc->RemoveTransformCallback(transformCallbackID);
}

std::vector<SoftEdge> SoftBodyComponent::GetEdgesFromMassAggregate() {
	std::vector<SoftEdge> dynamicEdges;
	if (MassAggregate.size() < 4) return dynamicEdges;

	int edgeCount = (int)MassAggregate.size() - 1;

	std::vector<glm::vec3> boundary(edgeCount);
	for (int i = 0; i < edgeCount; i++) boundary[i] = MassAggregate[i]->worldPos;

	for (int i = 0; i < edgeCount; i++) {
		int a = i;
		int b = (i + 1) % edgeCount;

		SoftEdge se;
		se.edge.start = MassAggregate[a]->worldPos;
		se.edge.end = MassAggregate[b]->worldPos;
		se.idxA = a;
		se.idxB = b;
		dynamicEdges.push_back(se);
	}
	return dynamicEdges;
}

void SoftBodyComponent::UpdateMassAggregate() {
	if (updatingFromParent || updatingFromPoints) return;
	updatingFromParent = true;

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	TransformComponent* tc = parent->GetComponent<TransformComponent>();

	for (int i = 0; i < rc->points.size(); i++)
	{
		glm::vec3 p = glm::vec3(rc->points[i][0], rc->points[i][1], 0.0f);
		MassAggregate[i]->UpdateWorldPosition(tc->ProjectToWorld(p));
	}

	MassAggregate[MassAggregate.size() - 1]->UpdateWorldPosition(tc->GetWorldPosition());
	updatingFromParent = false;
}

void SoftBodyComponent::SyncMeshFromMassAggregate() {
	if (updatingFromParent) return;
	if (MassAggregate.size() < 2) return;
	updatingFromPoints = true;

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	TransformComponent* tc = parent->GetComponent<TransformComponent>();

	PointMass* centerPM = MassAggregate.back().get();
	int edgeCount = (int)MassAggregate.size() - 1;

	glm::vec3 newOrigin = centerPM->worldPos;
	tc->UpdateWorldPosition(newOrigin);

	std::vector<float> verts = rc->Vertices;

	for (int i = 0; i < edgeCount; i++)
	{
		glm::vec3 worldP = MassAggregate[i]->worldPos;
		glm::vec3 localP = tc->ProjectToWorld(worldP, true);
		verts[i * 5] = localP.x;
		verts[i * 5 + 1] = localP.y;
	}

	if (std::holds_alternative<CircleShape>(rc->currentShape)) {
		CircleShape& shape = std::get<CircleShape>(rc->currentShape);
		glm::vec3 centerLocal = tc->ProjectToWorld(newOrigin, true); 
		int centerOffset = (int)verts.size() - 5;
		verts[centerOffset] = centerLocal.x;
		verts[centerOffset + 1] = centerLocal.y;
		rc->UpdateShape(verts, rc->TriangulateCircle(shape.segments));
	}
	else {
		rc->UpdateShape(verts, rc->Triangulate(verts));
	}

	VertexComponent* vc = parent->GetComponent<VertexComponent>();
	if (vc) {
		for (int i = 0; i < edgeCount; i++)
		{
			glm::vec3 worldP = MassAggregate[i]->worldPos;
			glm::vec3 localP = tc->ProjectToWorld(worldP, true);
			vc->vertexPoints[i]->UpdatePosition(localP.x, localP.y);
		}
	}

	updatingFromPoints = false;
}

void SoftBodyComponent::BuildMassAggregate() {
	MassAggregate.clear();
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	TransformComponent* tc = parent->GetComponent<TransformComponent>();

	int physicsPointCount = (int)rc->points.size();
	if (std::holds_alternative<CircleShape>(rc->currentShape)) {
		physicsPointCount -= 1;
	}

	for (int i = 0; i < physicsPointCount; i++)
	{
		glm::vec3 p = glm::vec3(rc->points[i][0], rc->points[i][1], 0.0f);
		std::unique_ptr<PointMass> pm = std::make_unique<PointMass>(Shader("vertex.txt", "fragment.txt"), this, tc->ProjectToWorld(p), i, false);
		PhysicsEngine::getInstance().allSoftBodyPointMasses.push_back(pm.get());
		MassAggregate.push_back(std::move(pm));
	}

	std::unique_ptr<PointMass> pm = std::make_unique<PointMass>(Shader("vertex.txt", "fragment.txt"), this, tc->GetWorldPosition(), MassAggregate.size(), true);
	PhysicsEngine::getInstance().allSoftBodyPointMasses.push_back(pm.get());
	MassAggregate.push_back(std::move(pm));

	for (int i = 0; i < MassAggregate.size(); i++)
	{
		MassAggregate[i]->inverseMass = inverseMass * MassAggregate.size();
	}

	int edgeCount = MassAggregate.size() - 1;
	CenterPM = MassAggregate.back().get();

	float compliance = (stiffness > 0.0f) ? (1.0f / stiffness) : 0.0f;

	// Structural springs (perimeter)
	for (int i = 0; i < edgeCount; i++)
	{
		PointMass* pmA = MassAggregate[i].get();
		PointMass* pmB = MassAggregate[(i + 1) % edgeCount].get();

		float restLength = glm::length(pmB->worldPos - pmA->worldPos);

		XPBDDistanceConstraint* constraint = new XPBDDistanceConstraint(
			pmA->body, pmB->body, restLength, compliance, damping);
		PhysicsEngine::getInstance().RegisterXPBDConstraint(constraint);
		springs.push_back(constraint);
	}

	// Spoke springs (center to each vertex)
	for (int i = 0; i < edgeCount; i++)
	{
		PointMass* pmV = MassAggregate[i].get();
		float restLength = glm::length(pmV->worldPos - CenterPM->worldPos);

		XPBDDistanceConstraint* constraint = new XPBDDistanceConstraint(
			CenterPM->body, pmV->body, restLength, compliance, damping);
		PhysicsEngine::getInstance().RegisterXPBDConstraint(constraint);
		springs.push_back(constraint);
	}

	// Shear springs
	if (edgeCount >= 4) {
		for (int i = 0; i < edgeCount; i++)
		{
			PointMass* pmA = MassAggregate[i].get();
			PointMass* pmB = MassAggregate[(i + 2) % edgeCount].get();
			float restLength = glm::length(pmB->worldPos - pmA->worldPos);

			XPBDDistanceConstraint* constraint = new XPBDDistanceConstraint(
				pmA->body, pmB->body, restLength, compliance, damping);
			PhysicsEngine::getInstance().RegisterXPBDConstraint(constraint);
			springs.push_back(constraint);
		}
	}
	
	// Apply area constraint
	std::vector<PhysicsBody> massBody;
	for (int i = 0; i < MassAggregate.size() - 1; i++)
	{
		massBody.push_back(MassAggregate[i]->body);
	}

	areaConstraint = new XPBDAreaConstraint(massBody,  (stiffness > 0.0f) ? (1.0f / stiffness) : 0.0f);
	PhysicsEngine::getInstance().RegisterXPBDConstraint(areaConstraint);
}

void SoftBodyComponent::DrawSprings() {
	auto& renderer = Renderer::getInstance();

	for (const auto* spring : springs) {
		glm::vec3 posA = *spring->objA.position;
		glm::vec3 posB = *spring->objB.position;

		float currentLength = glm::length(posA - posB);
		float ratio = currentLength / spring->restLength;

		glm::vec4 color;
		if (ratio > 1.05f) color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); 
		else if (ratio < 0.95f) color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); 
		else color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); 

		renderer.DrawLine(posA, posB, color);
	}
}