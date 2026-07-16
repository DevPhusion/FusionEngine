#include "FluidComponent.h"
#include "PhysicsEngine.h"

FluidComponent::FluidComponent(Object* parent) : ComponentBase<FluidComponent>(parent) {
	Name = "Fluid Component";

	SeedParticles();
	InitRenderResources();
	setShapeCallbackID = parent->GetComponent<RenderComponent>()->AddOnShapeSetCallback([this] {
		SeedParticles();
		ResizeInstanceBuffer();
		});
	transformCallbackID = parent->GetComponent<TransformComponent>()->AddTransformCallback([this] {
		UpdateParticleTransforms();
		});
}

void FluidComponent::ProcessFluid(float delta) {

}

void FluidComponent::SeedParticles() {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!rc || rc->points.empty()) return;

	float minX = rc->points[0][0], maxX = minX;
	float minY = rc->points[0][1], maxY = minY;
	for (auto& p : rc->points) {
		minX = std::min(minX, p[0]); maxX = std::max(maxX, p[0]);
		minY = std::min(minY, p[1]); maxY = std::max(maxY, p[1]);
	}

	float area = rc->GetArea();
	if (area <= 0.0f) area = (maxX - minX) * (maxY - minY);
	if (area <= 0.0f) return;

	float spacing = std::sqrt(area / std::max(1, desiredParticleCount));
	spacing = std::max(spacing, 0.001f);

	localParticlePositions.clear();
	for (float y = minY + spacing * 0.5f; y <= maxY; y += spacing) {
		for (float x = minX + spacing * 0.5f; x <= maxX; x += spacing) {
			glm::vec3 local(x, y, 0.0f);
			if (rc->IsInsideShape(local)) {
				localParticlePositions.push_back(local);
			}
		}
	}
	
	
	auto& allParticles = PhysicsEngine::getInstance().allFluidParticles;
	for (FluidParticle* p : particles) {
		allParticles.erase(std::remove(allParticles.begin(), allParticles.end(), p), allParticles.end());
	}
	for (FluidParticle* p : particles) {
		delete p;
	}
	particles.clear();
	particles.reserve(localParticlePositions.size());
	for (auto& localPos : localParticlePositions) {
		glm::vec3 worldPos = tc ? tc->ProjectToWorld(localPos) : localPos;

		FluidParticle* p = new FluidParticle();
		p->position = worldPos;
		p->predictedPosition = worldPos;
		p->prevPosition = worldPos;
		p->velocity = glm::vec3(0.0f);
		p->invMass = 1.0f;
		p->lambda = 0.0f;
		p->vorticity = glm::vec3(0.0f);
		PhysicsEngine::getInstance().allFluidParticles.push_back(p);
		particles.push_back(p);
	}
}

void FluidComponent::ProcessInspectorUI() {
	ImGui::Text("Particles Count");
	ImGui::SameLine();
	if (ImGui::InputInt("##ParticlesCount", &desiredParticleCount)) {
		desiredParticleCount = std::max(1, desiredParticleCount);
		SeedParticles();
		ResizeInstanceBuffer();
		EngineManager::getInstance().EngineChangeEvent();
	}

	ImGui::Text("Color");
	ImGui::SameLine();
	float displayColor[4] = { color.x, color.y, color.z, color.a };
	if (ImGui::ColorEdit4("##Color", displayColor)) {
		this->color = glm::vec4(displayColor[0], displayColor[1], displayColor[2], displayColor[3]);
		EngineManager::getInstance().EngineChangeEvent();
	}

	ImGui::Text("Particle Radius");
	ImGui::SameLine();
	if (ImGui::InputFloat("##ParticleRadius", &particleRadius)) {
		particleRadius = std::max(0.0001f, particleRadius);
		RebuildQuadGeometry();
		EngineManager::getInstance().EngineChangeEvent();
	}
}

void FluidComponent::OnDelete() {
	if (!renderInitialized) return;
	glDeleteBuffers(1, &quadVBO);
	glDeleteBuffers(1, &quadEBO);
	glDeleteBuffers(1, &instanceVBO);
	glDeleteVertexArrays(1, &quadVAO);
	renderInitialized = false;

	auto& allParticles = PhysicsEngine::getInstance().allFluidParticles;
	for (FluidParticle* p : particles) {
		allParticles.erase(std::remove(allParticles.begin(), allParticles.end(), p), allParticles.end());
	}
	for (FluidParticle* p : particles) {
		delete p;
	}
	particles.clear();

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc && setShapeCallbackID != -1) rc->RemoveOnShapeSetCallback(setShapeCallbackID);

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc && transformCallbackID != -1) tc->RemoveTransformCallback(transformCallbackID);
}

void FluidComponent::CopyTo(Object* other) {
	FluidComponent* target = other->GetComponent<FluidComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<FluidComponent>(other));
		target = other->GetComponent<FluidComponent>();
	}

	target->desiredParticleCount = desiredParticleCount;
	target->color = color;
	target->particleRadius = particleRadius;
	target->SeedParticles();
	target->ResizeInstanceBuffer();
	target->RebuildQuadGeometry();
}

std::unique_ptr<Component> FluidComponent::Clone(Object* parent) {
	std::unique_ptr<FluidComponent> comp = std::make_unique<FluidComponent>(parent);
	comp->desiredParticleCount = desiredParticleCount;
	comp->color = color;
	comp->particleRadius = particleRadius;
	comp->Enabled = false;
	return comp;
}

void FluidComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(desiredParticleCount);
	w.Write(color);
	w.Write(particleRadius);
}

void FluidComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	desiredParticleCount = r.Read<int>();
	color = r.Read<glm::vec4>();
	particleRadius = r.Read<float>();
	SeedParticles();
	ResizeInstanceBuffer();
	RebuildQuadGeometry();
}

void FluidComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (enabled) {
		SeedParticles();
		ResizeInstanceBuffer();
		RebuildQuadGeometry();
	}
}

void FluidComponent::Draw() {
	if (!renderInitialized || particles.empty()) return;
	if (!Enabled) return;

	UpdateInstanceBuffer();

	this->particleShader.use();
	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio, EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
	this->particleShader.setMat4D("projection", projection);
	this->particleShader.setMat4D("view", Camera::getInstance().viewMatrix);
	this->particleShader.setVec4D("aColor", this->color);

	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, (GLsizei)particles.size());
	glBindVertexArray(0);
}

void FluidComponent::InitRenderResources() {
	unsigned int quadIdx[] = { 0, 1, 2,  2, 3, 0 };

	particleShader = Shader("fluid_vertex.txt", "fluid_fragment.txt");

	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);

	glGenBuffers(1, &quadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW); // now dynamic, filled by RebuildQuadGeometry()

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &quadEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);

	glGenBuffers(1, &instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	renderInitialized = true;

	RebuildQuadGeometry();
}

void FluidComponent::RebuildQuadGeometry() {
	if (!renderInitialized) return;

	float h = particleRadius;
	float quadVerts[] = {
		-h, -h, 0.0f,   0.0f, 0.0f,
		 h, -h, 0.0f,   1.0f, 0.0f,
		 h,  h, 0.0f,   1.0f, 1.0f,
		-h,  h, 0.0f,   0.0f, 1.0f,
	};

	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FluidComponent::UpdateInstanceBuffer() {
	if (!renderInitialized) return;

	std::vector<glm::vec3> positions;
	positions.reserve(particles.size());
	for (auto& p : particles) positions.push_back(p->position);

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec3), positions.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FluidComponent::UpdateParticleTransforms() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;
	if (localParticlePositions.size() != particles.size()) return; 

	for (size_t i = 0; i < particles.size(); ++i) {
		glm::vec3 worldPos = tc->ProjectToWorld(localParticlePositions[i]);
		particles[i]->position = worldPos;
		particles[i]->predictedPosition = worldPos;
		particles[i]->prevPosition = worldPos; 
	}
}

void FluidComponent::ResizeInstanceBuffer() {
	if (!renderInitialized) return;
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}