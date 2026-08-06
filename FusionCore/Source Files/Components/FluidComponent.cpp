#include "../../Header Files/Components/FluidComponent.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"

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

	CollisionComponent* cc = parent->GetComponent<CollisionComponent>();
	if (cc) {
		TransformComponent* tc = parent->GetComponent<TransformComponent>();
		if (tc) tc->RemoveTransformCallback(cc->onTransformCallbackID);
		//PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
	}
}

void FluidComponent::ClearParticles() {
	auto& allParticles = PhysicsEngine::getInstance().allFluidParticles;
	for (FluidParticle* p : particles) {
		allParticles.erase(std::remove(allParticles.begin(), allParticles.end(), p), allParticles.end());
	}
	for (FluidParticle* p : particles) {
		delete p;
	}
	particles.clear();
}

FluidParticle* FluidComponent::AddParticle(glm::vec3 worldPosition) {
	FluidParticle* p = new FluidParticle();
	p->parent = parent;
	p->position = worldPosition;
	p->predictedPosition = worldPosition;
	p->velocity = glm::vec3(0.0f);
	p->collisionRadius = collisionRadius;
	p->mass = particleMass;
	p->invMass = particleMass > 0.0f ? 1.0f / particleMass : 0.0f;
	p->restDensity = restDensity;
	p->viscosity = viscosity;
	p->lambda = 0.0f;
	p->vorticityEps = vorticityStrength;
	p->epsilon = epsilon;
	p->smoothingRadius = smoothingRadius;
	p->poly6Coeff = PhysicsEngine::getInstance().Poly6Coefficient(smoothingRadius);
	p->spikyCoeff = PhysicsEngine::getInstance().SpikyCoefficient(smoothingRadius);
	p->density = 0.0f;

	CollisionComponent* cc = parent->GetComponent<CollisionComponent>();
	if (cc) {
		p->collisionLayer = cc->collisionLayer;
		p->collisionMask = cc->collisionMask;
	}

	PhysicsEngine::getInstance().allFluidParticles.push_back(p);
	particles.push_back(p);

	ResizeInstanceBuffer();
	return p;
}

std::vector<FluidParticle*> FluidComponent::AddParticles(Shape shape, int particleCount) {
	std::vector<FluidParticle*> added;
	particleCount = std::max(1, particleCount);

	glm::vec3 boundsMin, boundsMax;
	GetShapeBounds(shape, boundsMin, boundsMax);

	float width = boundsMax.x - boundsMin.x;
	float height = boundsMax.y - boundsMin.y;
	float area = width * height;
	if (area <= 0.0f) return added;

	float spacing = std::sqrt(area / (float)particleCount);
	spacing = std::max(spacing, 0.001f);

	for (float y = boundsMin.y + spacing * 0.5f; y <= boundsMax.y; y += spacing) {
		for (float x = boundsMin.x + spacing * 0.5f; x <= boundsMax.x; x += spacing) {
			glm::vec3 point(x, y, boundsMin.z);
			if (IsPointInsideShape(shape, point)) {
				added.push_back(AddParticle(point));
			}
		}
	}

	return added;
}

void FluidComponent::RemoveParticle(FluidParticle* particle) {
	if (!particle) return;

	auto it = std::find(particles.begin(), particles.end(), particle);
	if (it == particles.end()) return;

	size_t index = std::distance(particles.begin(), it);

	auto& allParticles = PhysicsEngine::getInstance().allFluidParticles;
	allParticles.erase(std::remove(allParticles.begin(), allParticles.end(), particle), allParticles.end());

	delete particle;
	particles.erase(it);
	if (index < localParticlePositions.size()) {
		localParticlePositions.erase(localParticlePositions.begin() + index);
	}

	ResizeInstanceBuffer();
}

void FluidComponent::SeedParticles() {
	if (!Enabled) return;

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

	ClearParticles();
	particles.reserve(localParticlePositions.size());
	CollisionComponent* cc = parent->GetComponent<CollisionComponent>();
	for (auto& localPos : localParticlePositions) {
		glm::vec3 worldPos = tc ? tc->ProjectToWorld(localPos) : localPos;

		FluidParticle* p = new FluidParticle();
		p->parent = parent;
		p->position = worldPos;
		p->predictedPosition = worldPos;
		p->velocity = glm::vec3(0.0f);
		p->collisionRadius = collisionRadius;
		p->mass = particleMass;
		p->invMass = 1 / p->mass;
		p->restDensity = restDensity;
		p->viscosity = viscosity;
		p->lambda = 0.0f;
		p->vorticityEps = vorticityStrength;
		p->epsilon = epsilon;
		p->smoothingRadius = smoothingRadius;
		p->poly6Coeff = PhysicsEngine::getInstance().Poly6Coefficient(smoothingRadius);
		p->spikyCoeff = PhysicsEngine::getInstance().SpikyCoefficient(smoothingRadius);
		if (cc) {
			p->collisionLayer = p->collisionLayer;
			p->collisionMask = p->collisionMask;
		}
		PhysicsEngine::getInstance().allFluidParticles.push_back(p);
		particles.push_back(p);
	}
}

void FluidComponent::ProcessInspectorUI() {
	if (ImGui::TreeNodeEx("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Color");
		ImGui::SameLine();
		float displayColor[4] = { color.x, color.y, color.z, color.a };
		if (ImGui::ColorEdit4("##Color", displayColor)) {
			this->color = glm::vec4(displayColor[0], displayColor[1], displayColor[2], displayColor[3]);
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Outline Color");
		ImGui::SameLine();
		float displayOutline[4] = { outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.a };
		if (ImGui::ColorEdit4("##OutlineColor", displayOutline)) {
			this->outlineColor = glm::vec4(displayOutline[0], displayOutline[1], displayOutline[2], displayOutline[3]);
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Particle Radius");
		ImGui::SameLine();
		if (ImGui::InputFloat("##ParticleRadius", &particleRadius)) {
			particleRadius = std::max(0.0001f, particleRadius);
			RebuildDensityQuadGeometry();
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Metaball Threshold");
		ImGui::SameLine();
		ImGui::InputFloat("##MetaballThreshold", &metaballThreshold);

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Fluid Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Particles Count");
		ImGui::SameLine();
		if (ImGui::InputInt("##ParticlesCount", &desiredParticleCount)) {
			desiredParticleCount = std::max(1, desiredParticleCount);
			SeedParticles();
			ResizeInstanceBuffer();
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Collision Radius");
		ImGui::SameLine();
		if (ImGui::InputFloat("##CollisionRadius", &collisionRadius)) {
			collisionRadius = std::max(0.0001f, collisionRadius);
			for (int i = 0; i < particles.size(); i++)
				particles[i]->collisionRadius = collisionRadius;
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Smoothing Radius");
		ImGui::SameLine();
		if (ImGui::InputFloat("##SmoothingRadius", &smoothingRadius)) {
			smoothingRadius = std::max(0.0001f, smoothingRadius);
			for (int i = 0; i < particles.size(); i++)
			{
				particles[i]->smoothingRadius = smoothingRadius;
				particles[i]->poly6Coeff = PhysicsEngine::getInstance().Poly6Coefficient(smoothingRadius);
				particles[i]->spikyCoeff = PhysicsEngine::getInstance().SpikyCoefficient(smoothingRadius);
			}
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Epsilon");
		ImGui::SameLine();
		if (ImGui::InputFloat("##Epsilon", &epsilon)) {
			epsilon = std::max(0.0001f, epsilon);
			for (int i = 0; i < particles.size(); i++)
			{
				particles[i]->epsilon = epsilon;
			}
			EngineManager::getInstance().EngineChangeEvent();
		}

		ImGui::Text("Particle Mass");
		ImGui::SameLine();
		if (ImGui::InputFloat("##ParticleMass", &particleMass, 0.0f, 0.0f, "%.3f kg")) {
			for (int i = 0; i < particles.size(); i++)
			{
				if (particleMass <= 0) particleMass = 0.01f;
				particles[i]->mass = particleMass;
				particles[i]->invMass = 1.0f / particleMass;
			}
		}

		ImGui::Text("Density");
		ImGui::SameLine();
		if (ImGui::InputFloat("##Density", &restDensity, 0.0f, 0.0f, "%.3f kg/m³")) {
			for (int i = 0; i < particles.size(); i++)
			{
				if (restDensity <= 0) restDensity = 0.01f;
				particles[i]->restDensity = restDensity;
			}
		}

		ImGui::Text("Viscosity");
		ImGui::SameLine();
		if (ImGui::InputFloat("##Viscosity", &viscosity)) {
			for (int i = 0; i < particles.size(); i++)
			{
				if (viscosity <= 0) viscosity = 0.01f;
				particles[i]->viscosity = viscosity;
			}
		}

		ImGui::Text("Vorticity Strength");
		ImGui::SameLine();
		if (ImGui::InputFloat("##Vorticity Strength", &vorticityStrength)) {
			for (int i = 0; i < particles.size(); i++)
			{
				if (vorticityStrength <= 0) vorticityStrength = 0.0f;
				particles[i]->vorticityEps = vorticityStrength;
			}
		}

		ImGui::TreePop();
	}
}

void FluidComponent::OnDelete() {
	auto& allParticles = PhysicsEngine::getInstance().allFluidParticles;
	std::unordered_set<FluidParticle*> toRemove(particles.begin(), particles.end());
	allParticles.erase(
		std::remove_if(allParticles.begin(), allParticles.end(),
			[&](FluidParticle* p) { return toRemove.count(p) != 0; }),
		allParticles.end());

	for (FluidParticle* p : particles) delete p;
	particles.clear();

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc && setShapeCallbackID != -1) rc->RemoveOnShapeSetCallback(setShapeCallbackID);

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc && transformCallbackID != -1) tc->RemoveTransformCallback(transformCallbackID);

	if (!renderInitialized) return;   

	glDeleteBuffers(1, &quadVBO);
	glDeleteBuffers(1, &quadEBO);
	glDeleteBuffers(1, &instanceVBO);
	glDeleteVertexArrays(1, &quadVAO);
	renderInitialized = false;

	if (densityInitialized) {
		glDeleteFramebuffers(1, &densityFBO);
		glDeleteTextures(1, &densityTex);
		densityInitialized = false;
	}
	glDeleteBuffers(1, &densityQuadVBO);
	glDeleteVertexArrays(1, &densityQuadVAO);
	glDeleteBuffers(1, &fsQuadVBO);
	glDeleteVertexArrays(1, &fsQuadVAO);
	glDeleteBuffers(1, &solidMaskVBO);
	glDeleteVertexArrays(1, &solidMaskVAO);
	glDeleteBuffers(1, &heatVBO);
	glDeleteBuffers(1, &vectorFieldVBO);
	glDeleteVertexArrays(1, &vectorFieldVAO);
}

void FluidComponent::CopyTo(Object* other) {
	FluidComponent* target = other->GetComponent<FluidComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<FluidComponent>(other));
		target = other->GetComponent<FluidComponent>();
	}

	target->desiredParticleCount = desiredParticleCount;
	target->color = color;
	target->outlineColor = outlineColor;
	target->particleRadius = particleRadius;
	target->collisionRadius = collisionRadius;
	target->particleMass = particleMass;
	target->restDensity = restDensity;
	target->viscosity = viscosity;
	target->vorticityStrength = vorticityStrength;
	target->epsilon = epsilon;
	target->smoothingRadius = smoothingRadius;
	target->SeedParticles();
	target->ResizeInstanceBuffer();
	target->RebuildQuadGeometry();
	target->RebuildDensityQuadGeometry();
}

std::unique_ptr<Component> FluidComponent::Clone(Object* parent) {
	std::unique_ptr<FluidComponent> comp = std::make_unique<FluidComponent>(parent);
	comp->desiredParticleCount = desiredParticleCount;
	comp->color = color;
	comp->outlineColor = outlineColor;
	comp->particleRadius = particleRadius;
	comp->collisionRadius = collisionRadius;
	comp->particleMass = particleMass;
	comp->restDensity = restDensity;
	comp->viscosity = viscosity;
	comp->vorticityStrength = vorticityStrength;
	comp->epsilon = epsilon;
	comp->smoothingRadius = smoothingRadius;
	comp->SetEnabled(false);
	return comp;
}

void FluidComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(desiredParticleCount);
	w.Write(color);
	w.Write(outlineColor);
	w.Write(particleRadius);
	w.Write(collisionRadius);
	w.Write(particleMass);
	w.Write(restDensity);
	w.Write(viscosity);
	w.Write(vorticityStrength);
	w.Write(epsilon);
	w.Write(smoothingRadius);
}

void FluidComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	desiredParticleCount = r.Read<int>();
	color = r.Read<glm::vec4>();
	outlineColor = r.Read<glm::vec4>();
	particleRadius = r.Read<float>();
	collisionRadius = r.Read<float>();
	particleMass = r.Read<float>();
	restDensity = r.Read<float>();
	viscosity = r.Read<float>();
	vorticityStrength = r.Read<float>();
	epsilon = r.Read<float>();
	smoothingRadius = r.Read<float>();
	SeedParticles();
	ResizeInstanceBuffer();
	RebuildQuadGeometry();
	RebuildDensityQuadGeometry();
}

void FluidComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (enabled) {
		SeedParticles();
		ResizeInstanceBuffer();
		RebuildQuadGeometry();
		RebuildDensityQuadGeometry();
	}
	else {
		ClearParticles();
	}
}

void FluidComponent::Draw() {
	if (!renderInitialized || particles.empty()) return;
	if (!Enabled) return;

	UpdateInstanceBuffer();

	if (EngineManager::getInstance().EngineSettings.drawFluidsAsParticles) {
		DrawParticlesDebug();
		return;
	}

	if (!densityInitialized) {
		GLint vp[4];
		glGetIntegerv(GL_VIEWPORT, vp);
		if (vp[2] > 0 && vp[3] > 0) {
			ResizeRenderTargets(vp[2], vp[3]);
		}
		if (!densityInitialized) return;
	}

	DrawDensityPass();
	DrawComposite();
}

void FluidComponent::InitRenderResources() {
	unsigned int quadIdx[] = { 0, 1, 2,  2, 3, 0 };

	particleShader = Shader("Resources/Shaders/Fluid/fluid_vertex.txt", "Resources/Shaders/Fluid/fluid_fragment.txt");
	densityShader = Shader("Resources/Shaders/Fluid/fluid_density_vertex.txt", "Resources/Shaders/Fluid/fluid_density_fragment.txt");
	compositeShader = Shader("Resources/Shaders/Fluid/fluid_composite_vertex.txt", "Resources/Shaders/Fluid/fluid_composite_fragment.txt");
	solidMaskShader = Shader("Resources/Shaders/Fluid/fluid_solidmask_vertex.txt", "Resources/Shaders/Fluid/fluid_solidmask_fragment.txt");
	vectorFieldShader = Shader("Resources/Shaders/Fluid/fluid_vector_vertex.txt", "Resources/Shaders/Fluid/fluid_vector_fragment.txt");

	glGenVertexArrays(1, &solidMaskVAO);
	glGenBuffers(1, &solidMaskVBO);
	glBindVertexArray(solidMaskVAO);
	glBindBuffer(GL_ARRAY_BUFFER, solidMaskVBO);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW); // sized per-draw
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

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

	glGenBuffers(1, &heatVBO);
	glBindBuffer(GL_ARRAY_BUFFER, heatVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
	glEnableVertexAttribArray(3);
	glVertexAttribDivisor(3, 1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	renderInitialized = true;

	RebuildQuadGeometry();

	glGenVertexArrays(1, &densityQuadVAO);
	glBindVertexArray(densityQuadVAO);

	glGenBuffers(1, &densityQuadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, densityQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO); // reuse the same 6 indices

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	RebuildDensityQuadGeometry();
	InitFullscreenQuad();
	InitVectorFieldResources();
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

void FluidComponent::RebuildDensityQuadGeometry() {
	if (!renderInitialized || densityQuadVBO == 0) return;

	float h = particleRadius;
	float quadVerts[] = {
		-h, -h, 0.0f,   0.0f, 0.0f,
		 h, -h, 0.0f,   1.0f, 0.0f,
		 h,  h, 0.0f,   1.0f, 1.0f,
		-h,  h, 0.0f,   0.0f, 1.0f,
	};

	glBindBuffer(GL_ARRAY_BUFFER, densityQuadVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FluidComponent::InitFullscreenQuad() {
	float verts[] = {
		-1.0f, -1.0f,   0.0f, 0.0f,
		 1.0f, -1.0f,   1.0f, 0.0f,
		 1.0f,  1.0f,   1.0f, 1.0f,
		-1.0f, -1.0f,   0.0f, 0.0f,
		 1.0f,  1.0f,   1.0f, 1.0f,
		-1.0f,  1.0f,   0.0f, 1.0f,
	};

	glGenVertexArrays(1, &fsQuadVAO);
	glGenBuffers(1, &fsQuadVBO);

	glBindVertexArray(fsQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, fsQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void FluidComponent::InitDensityFBO(int width, int height) {
	width = std::max(1, width);
	height = std::max(1, height);
	densityW = width;
	densityH = height;

	glGenFramebuffers(1, &densityFBO);
	glGenTextures(1, &densityTex);

	glBindTexture(GL_TEXTURE_2D, densityTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, densityFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, densityTex, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	densityInitialized = true;
}

void FluidComponent::InitVectorFieldResources() {
	glGenVertexArrays(1, &vectorFieldVAO);
	glGenBuffers(1, &vectorFieldVBO);

	glBindVertexArray(vectorFieldVAO);
	glBindBuffer(GL_ARRAY_BUFFER, vectorFieldVBO);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW); // sized per-draw

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void FluidComponent::ResizeRenderTargets(int width, int height) {
	if (width <= 0 || height <= 0) return;
	if (densityInitialized && width == densityW && height == densityH) return;

	if (densityInitialized) {
		glDeleteFramebuffers(1, &densityFBO);
		glDeleteTextures(1, &densityTex);
		densityInitialized = false;
	}
	InitDensityFBO(width, height);
}

void FluidComponent::DrawObjectSilhouette(const RigidBoundary& rb) {
	if (rb.worldEdges.size() < 3) return;

	glm::vec3 centroid(0.0f);
	for (auto& e : rb.worldEdges) centroid += e.start;
	centroid /= (float)rb.worldEdges.size();

	std::vector<float> verts;
	verts.reserve((rb.worldEdges.size() + 2) * 3);
	verts.insert(verts.end(), { centroid.x, centroid.y, centroid.z });
	for (auto& e : rb.worldEdges) verts.insert(verts.end(), { e.start.x, e.start.y, e.start.z });
	verts.insert(verts.end(), { rb.worldEdges[0].start.x, rb.worldEdges[0].start.y, rb.worldEdges[0].start.z });

	glBindBuffer(GL_ARRAY_BUFFER, solidMaskVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

	glBindVertexArray(solidMaskVAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)(verts.size() / 3));
	glBindVertexArray(0);
}

void FluidComponent::DrawObjectSilhouette(const SoftBoundary& soft) {
	if (soft.worldEdges.size() < 3) return;

	glm::vec3 centroid(0.0f);
	for (auto& e : soft.worldEdges) centroid += e.edge.start;
	centroid /= (float)soft.worldEdges.size();

	std::vector<float> verts;
	verts.reserve((soft.worldEdges.size() + 2) * 3);
	verts.insert(verts.end(), { centroid.x, centroid.y, centroid.z });
	for (auto& e : soft.worldEdges) verts.insert(verts.end(), { e.edge.start.x, e.edge.start.y, e.edge.start.z });
	verts.insert(verts.end(), { soft.worldEdges[0].edge.start.x, soft.worldEdges[0].edge.start.y, soft.worldEdges[0].edge.start.z });

	glBindBuffer(GL_ARRAY_BUFFER, solidMaskVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

	glBindVertexArray(solidMaskVAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)(verts.size() / 3));
	glBindVertexArray(0);
}

void FluidComponent::DrawDensityPass() {
	if (!densityInitialized) return;

	GLint prevViewport[4];
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	GLint prevFBO = 0;                                  // NEW
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);     // NEW - capture whatever's actually bound (the game's Viewport FBO)

	glBindFramebuffer(GL_FRAMEBUFFER, densityFBO);
	glViewport(0, 0, densityW, densityH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio,
		EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f); // also fixed aspect source, see below
	glm::mat4 view = Camera::getInstance().viewMatrix;

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	densityShader.use();
	densityShader.setMat4D("projection", projection);
	densityShader.setMat4D("view", view);

	glBindVertexArray(densityQuadVAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, (GLsizei)particles.size());
	glBindVertexArray(0);

	auto overlappingRigid = GetOverlappingRigidBodies();
	auto overlappingSoft = GetOverlappingSoftBodies();
	if (!overlappingRigid.empty() || !overlappingSoft.empty()) {
		glBlendEquation(GL_MAX);
		glBlendFunc(GL_ONE, GL_ONE);

		solidMaskShader.use();
		solidMaskShader.setMat4D("projection", projection);
		solidMaskShader.setMat4D("view", view);
		solidMaskShader.setFloat("softness", particleRadius);

		for (const RigidBoundary* rb : overlappingRigid) {
			float waterLine = GetWaterLine(*rb);
			if (waterLine == -INFINITY) continue;
			solidMaskShader.setFloat("waterLevel", waterLine);
			DrawObjectSilhouette(*rb);
		}
		for (const SoftBoundary* sb : overlappingSoft) {
			float waterLine = GetWaterLine(*sb);
			if (waterLine == -INFINITY) continue;
			solidMaskShader.setFloat("waterLevel", waterLine);
			DrawObjectSilhouette(*sb);
		}

		glBlendEquation(GL_FUNC_ADD);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);   
	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void FluidComponent::DrawComposite() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // normal alpha blend over the scene

	compositeShader.use();
	compositeShader.setInt("densityTex", 0);
	compositeShader.setVec4D("fillColor", this->color);
	compositeShader.setVec4D("outlineColor", this->outlineColor);
	compositeShader.setFloat("threshold", metaballThreshold);
	compositeShader.setFloat("edgeSoft", metaballEdgeSoft);
	compositeShader.setFloat("outlineWidth", outlineWidthTexels);
	compositeShader.setVec2D("texelSize", glm::vec2(1.0f / densityW, 1.0f / densityH));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, densityTex);

	glBindVertexArray(fsQuadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec4 FluidComponent::VelocityHeatmapColor(float t) {
	t = glm::clamp(t, 0.0f, 1.0f);

	glm::vec3 c0(0.05f, 0.05f, 0.35f);
	glm::vec3 c1(0.0f, 0.75f, 0.9f);  
	glm::vec3 c2(0.2f, 0.85f, 0.2f);  
	glm::vec3 c3(0.95f, 0.85f, 0.1f);  
	glm::vec3 c4(0.9f, 0.15f, 0.1f);  

	glm::vec3 color;
	if (t < 0.25f)      color = glm::mix(c0, c1, t / 0.25f);
	else if (t < 0.5f)  color = glm::mix(c1, c2, (t - 0.25f) / 0.25f);
	else if (t < 0.75f) color = glm::mix(c2, c3, (t - 0.5f) / 0.25f);
	else                color = glm::mix(c3, c4, (t - 0.75f) / 0.25f);

	return glm::vec4(color, 1.0f);
}

void FluidComponent::DrawVelocityField() {
	if (!vectorFieldVAO || particles.empty()) return;

	float maxSpeed = 0.0001f;
	for (auto* p : particles) maxSpeed = std::max(maxSpeed, glm::length(p->velocity));

	std::vector<float> lineVerts;
	lineVerts.reserve(particles.size() * 6 * 7);

	auto pushVert = [&](const glm::vec3& pos, const glm::vec4& col) {
		lineVerts.insert(lineVerts.end(), { pos.x, pos.y, pos.z, col.r, col.g, col.b, col.a });
		};

	const glm::vec3 defaultDir(1.0f, 0.0f, 0.0f); 

	for (auto* p : particles) {
		float speed = glm::length(p->velocity);
		float t = speed / maxSpeed; 

		glm::vec3 dir = (speed > 1e-8f) ? (p->velocity / speed) : defaultDir;
		glm::vec4 color = VelocityHeatmapColor(t);

		glm::vec3 start = p->position;
		glm::vec3 end = start + dir * particleRadius; 

		pushVert(start, color);
		pushVert(end, color);

		glm::vec3 perp(-dir.y, dir.x, 0.0f);
		float headLen = particleRadius * 0.35f;
		glm::vec3 headBase = end - dir * headLen;

		pushVert(end, color);
		pushVert(headBase + perp * headLen * 0.5f, color);

		pushVert(end, color);
		pushVert(headBase - perp * headLen * 0.5f, color);
	}

	if (lineVerts.empty()) return;

	glBindBuffer(GL_ARRAY_BUFFER, vectorFieldVBO);
	glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(float), lineVerts.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio, EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
	glm::mat4 view = Camera::getInstance().viewMatrix;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(1.5f);

	vectorFieldShader.use();
	vectorFieldShader.setMat4D("projection", projection);
	vectorFieldShader.setMat4D("view", view);

	glBindVertexArray(vectorFieldVAO);
	glDrawArrays(GL_LINES, 0, (GLsizei)(lineVerts.size() / 7));
	glBindVertexArray(0);
}

void FluidComponent::DrawParticlesDebug() {
	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio, EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
	glm::mat4 view = Camera::getInstance().viewMatrix;

	bool showVectorField = EngineManager::getInstance().EngineSettings.drawFluidsVelocityField;

	if (!showVectorField) {
		FluidHeatmapMode heatmapMode = EngineManager::getInstance().EngineSettings.fluidHeatmapMode;
		bool heatmap = heatmapMode != FluidHeatmapMode::None;
		if (heatmap) UpdateHeatBuffer();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		particleShader.use();
		particleShader.setMat4D("projection", projection);
		particleShader.setMat4D("view", view);
		particleShader.setVec4D("aColor", this->color);
		particleShader.setBool("useHeatmap", heatmap);

		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, (GLsizei)particles.size());
		glBindVertexArray(0);
	}
	else
	{
		DrawVelocityField();
	}
}

void FluidComponent::UpdateCollisionLayerMask() {
	CollisionComponent* cc = parent->GetComponent<CollisionComponent>();
	if (cc) {
		for (auto* p : particles) {
			p->collisionLayer = cc->collisionLayer;
			p->collisionMask = cc->collisionMask;
		}
	}
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

void FluidComponent::UpdateHeatBuffer() {
	if (!renderInitialized) return;

	FluidHeatmapMode mode = EngineManager::getInstance().EngineSettings.fluidHeatmapMode;

	std::vector<float> rawValues;
	rawValues.reserve(particles.size());

	if (mode == FluidHeatmapMode::Velocity) {
		for (auto* p : particles) rawValues.push_back(glm::length(p->velocity));
	}
	else if (mode == FluidHeatmapMode::Density) {
		for (auto* p : particles) rawValues.push_back(p->density);
	}
	else {
		return; 
	}

	float maxVal = 0.0001f;
	for (float v : rawValues) maxVal = std::max(maxVal, v);

	std::vector<float> heatValues;
	heatValues.reserve(rawValues.size());
	for (float v : rawValues) heatValues.push_back(v / maxVal);

	glBindBuffer(GL_ARRAY_BUFFER, heatVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, heatValues.size() * sizeof(float), heatValues.data());
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
	}
}

void FluidComponent::ResizeInstanceBuffer() {
	if (!renderInitialized) return;
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, heatVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::vector<const RigidBoundary*> FluidComponent::GetOverlappingRigidBodies() {
	std::vector<const RigidBoundary*> result;
	if (particles.empty()) return result;

	glm::vec3 fMin(INFINITY), fMax(-INFINITY);
	for (auto* p : particles) {
		glm::vec3 r(p->collisionRadius);
		fMin = glm::min(fMin, p->position - r);
		fMax = glm::max(fMax, p->position + r);
	}

	for (const RigidBoundary& rb : PhysicsEngine::getInstance().rigidBoundaries) {
		if (rb.obj == parent) continue; 

		glm::vec3 rMin(INFINITY), rMax(-INFINITY);
		for (auto& e : rb.worldEdges) {
			rMin = glm::min(rMin, glm::min(e.start, e.end));
			rMax = glm::max(rMax, glm::max(e.start, e.end));
		}
		if (rMin.x > fMax.x || rMax.x < fMin.x) continue;
		if (rMin.y > fMax.y || rMax.y < fMin.y) continue;

		result.push_back(&rb);
	}
	return result;
}

std::vector<const SoftBoundary*> FluidComponent::GetOverlappingSoftBodies() {
	std::vector<const SoftBoundary*> result;
	if (particles.empty()) return result;

	glm::vec3 fMin(INFINITY), fMax(-INFINITY);
	for (auto* p : particles) {
		glm::vec3 r(p->collisionRadius);
		fMin = glm::min(fMin, p->position - r);
		fMax = glm::max(fMax, p->position + r);
	}

	for (const SoftBoundary& sb : PhysicsEngine::getInstance().softBoundaries) {
		if (sb.obj == parent) continue;
		if (sb.worldEdges.empty()) continue;

		glm::vec3 sMin(INFINITY), sMax(-INFINITY);
		for (auto& e : sb.worldEdges) {
			sMin = glm::min(sMin, glm::min(e.edge.start, e.edge.end));
			sMax = glm::max(sMax, glm::max(e.edge.start, e.edge.end));
		}
		if (sMin.x > fMax.x || sMax.x < fMin.x) continue;
		if (sMin.y > fMax.y || sMax.y < fMin.y) continue;

		result.push_back(&sb);
	}
	return result;
}

float FluidComponent::GetWaterLine(const RigidBoundary& rb) {
	if (rb.worldEdges.empty() || particles.empty()) return -INFINITY;

	float coverage2 = particleRadius * particleRadius;
	float highestY = -INFINITY;
	bool anyCovered = false;

	for (auto& e : rb.worldEdges) {
		bool vertexCovered = false;
		for (auto* p : particles) {
			glm::vec3 d = p->position - e.start;
			if ((d.x * d.x + d.y * d.y) <= coverage2) {
				vertexCovered = true;
				break;
			}
		}
		if (vertexCovered) {
			anyCovered = true;
			highestY = std::max(highestY, e.start.y);
		}
	}

	return anyCovered ? highestY : -INFINITY;
}

float FluidComponent::GetWaterLine(const SoftBoundary& soft) {
	if (soft.worldEdges.empty() || particles.empty()) return -INFINITY;

	float coverage2 = particleRadius * particleRadius;
	float highestY = -INFINITY;
	bool anyCovered = false;

	for (auto& e : soft.worldEdges) {
		bool vertexCovered = false;
		for (auto* p : particles) {
			glm::vec3 d = p->position - e.edge.start;
			if ((d.x * d.x + d.y * d.y) <= coverage2) {
				vertexCovered = true;
				break;
			}
		}
		if (vertexCovered) {
			anyCovered = true;
			highestY = std::max(highestY, e.edge.start.y);
		}
	}

	return anyCovered ? highestY : -INFINITY;
}

void FluidComponent::GetShapeBounds(const Shape& shape, glm::vec3& outMin, glm::vec3& outMax) {
	std::visit([&](auto&& s) {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape>) {
			outMin = s.center - glm::vec3(s.width * 0.5f, s.height * 0.5f, 0.0f);
			outMax = s.center + glm::vec3(s.width * 0.5f, s.height * 0.5f, 0.0f);
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			outMin = s.center - glm::vec3(s.radius, s.radius, 0.0f);
			outMax = s.center + glm::vec3(s.radius, s.radius, 0.0f);
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			if (s.vertices.size() < 5) { outMin = outMax = glm::vec3(0.0f); return; }
			outMin = glm::vec3(s.vertices[0], s.vertices[1], s.vertices[2]);
			outMax = outMin;
			for (size_t i = 0; i + 4 < s.vertices.size(); i += 5) {
				glm::vec3 v(s.vertices[i], s.vertices[i + 1], s.vertices[i + 2]);
				outMin = glm::min(outMin, v);
				outMax = glm::max(outMax, v);
			}
		}
		}, shape);
}

bool FluidComponent::IsPointInsideShape(const Shape& shape, const glm::vec3& point) {
	return std::visit([&](auto&& s) -> bool {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape>) {
			return std::abs(point.x - s.center.x) <= s.width * 0.5f &&
				std::abs(point.y - s.center.y) <= s.height * 0.5f;
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			glm::vec2 d(point.x - s.center.x, point.y - s.center.y);
			return glm::dot(d, d) <= s.radius * s.radius;
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			bool inside = false;
			size_t vertCount = s.vertices.size() / 5;
			for (size_t i = 0, j = vertCount - 1; i < vertCount; j = i++) {
				float xi = s.vertices[i * 5 + 0], yi = s.vertices[i * 5 + 1];
				float xj = s.vertices[j * 5 + 0], yj = s.vertices[j * 5 + 1];
				bool intersect = ((yi > point.y) != (yj > point.y)) &&
					(point.x < (xj - xi) * (point.y - yi) / (yj - yi) + xi);
				if (intersect) inside = !inside;
			}
			return inside;
		}
		return false;
		}, shape);
}