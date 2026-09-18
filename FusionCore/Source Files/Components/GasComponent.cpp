#include "../../Header Files/Components/GasComponent.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"
#include "../../Header Files/Core/Editor/EditorField.h"


GasComponent::GasComponent(Object* parent) : ComponentBase<GasComponent>(parent) {
	Name = "Gas Component";

	if (!EngineManager::getInstance().isHeadless) {
		InitRenderResources();
	}
}

void GasComponent::EnsureGLResources() {
	if (renderInitialized) return;
	InitRenderResources();
}

void GasComponent::Activate() {
	Component::Activate();

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
	}

	SeedParticles();
	ResizeInstanceBuffer();
}

void GasComponent::Deactivate() {
	Component::Deactivate();

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc && setShapeCallbackID != -1) rc->RemoveOnShapeSetCallback(setShapeCallbackID);
	setShapeCallbackID = -1;

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc && transformCallbackID != -1) tc->RemoveTransformCallback(transformCallbackID);
	transformCallbackID = -1;

	ClearParticles();
	isActive = false;
}

void GasComponent::ClearParticles() {
	auto& allParticles = PhysicsEngine::getInstance().allContinuumParticles;
	for (GasParticle* p : particles) {
		allParticles.erase(
			std::remove_if(allParticles.begin(), allParticles.end(),
				[p](const ContinuumParticle& v) {
					return std::visit([p](auto&& stored) -> bool {
						using T = std::decay_t<decltype(stored)>;
						if constexpr (std::is_same_v<T, GasParticle*>) {
							return stored == p;
						}
						else {
							return false;
						}
						}, v);
				}),
			allParticles.end());
	}
	for (GasParticle* p : particles) {
		delete p;
	}
	particles.clear();
}

void GasComponent::SeedParticles() {
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

		GasParticle* p = new GasParticle();
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
		p->stiffness = stiffness;
		p->gamma = gamma;
		p->temperature = initialTemperature;
		p->ambientTemperature = ambientTemperature;
		p->dissipationRate = dissipationRate;
		p->poly6Coeff = PhysicsEngine::getInstance().Poly6Coefficient(smoothingRadius);
		p->spikyCoeff = PhysicsEngine::getInstance().SpikyCoefficient(smoothingRadius);
		if (cc) {
			p->collisionLayer = cc->collisionLayer;
			p->collisionMask = cc->collisionMask;
		}
		PhysicsEngine::getInstance().allContinuumParticles.push_back(p);
		particles.push_back(p);
	}
}

void GasComponent::ProcessInspectorUI() {
	if (ImGui::TreeNodeEx("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
		float displayColor[4] = { color.x, color.y, color.z, color.a };
		EditorField::ColorEdit4Scene(parent, "Color", "##Color", displayColor, [&] {
			this->color = glm::vec4(displayColor[0], displayColor[1], displayColor[2], displayColor[3]);
			EngineManager::getInstance().SceneChangeEvent();
			});

		float displayOutline[4] = { outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.a };
		EditorField::ColorEdit4Scene(parent, "Outline Color", "##OutlineColor", displayOutline, [&] {
			this->outlineColor = glm::vec4(displayOutline[0], displayOutline[1], displayOutline[2], displayOutline[3]);
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Particle Radius", "##ParticleRadius", &particleRadius, [&] {
			particleRadius = std::max(0.0001f, particleRadius);
			RebuildDensityQuadGeometry();
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Metaball Threshold", "##MetaballThreshold", &metaballThreshold, [] {});

		EditorField::InputFloatScene(parent, "Noise Scale", "##NoiseScale", &noiseScale, [] {});
		
		EditorField::InputFloatScene(parent, "Noise Strength", "##NoiseStrength", &noiseStrength, [] {});
		
		EditorField::InputFloatScene(parent, "Rise Speed", "##RiseSpeed", &riseSpeed, [] {});

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Gas Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
		EditorField::InputIntScene(parent, "Particles Count", "##ParticlesCount", &desiredParticleCount, [&] {
			desiredParticleCount = std::max(1, desiredParticleCount);
			SeedParticles();
			ResizeInstanceBuffer();
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Collision Radius", "##CollisionRadius", &collisionRadius, [&] {
			collisionRadius = std::max(0.0001f, collisionRadius);
			for (int i = 0; i < particles.size(); i++) particles[i]->collisionRadius = collisionRadius;
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Smoothing Radius", "##SmoothingRadius", &smoothingRadius, [&] {
			smoothingRadius = std::max(0.0001f, smoothingRadius);
			for (int i = 0; i < particles.size(); i++) {
				particles[i]->smoothingRadius = smoothingRadius;
				particles[i]->poly6Coeff = PhysicsEngine::getInstance().Poly6Coefficient(smoothingRadius);
				particles[i]->spikyCoeff = PhysicsEngine::getInstance().SpikyCoefficient(smoothingRadius);
			}
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Epsilon", "##Epsilon", &epsilon, [&] {
			epsilon = std::max(0.0001f, epsilon);
			for (int i = 0; i < particles.size(); i++) particles[i]->epsilon = epsilon;
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Particle Mass", "##ParticleMass", &particleMass, [&] {
			if (particleMass <= 0) particleMass = 0.01f;
			for (int i = 0; i < particles.size(); i++) {
				particles[i]->mass = particleMass;
				particles[i]->invMass = 1.0f / particleMass;
			}
			}, "%.3f kg");

		EditorField::InputFloatScene(parent, "Ambient Density", "##Density", &restDensity, [&] {
			if (restDensity <= 0) restDensity = 0.01f;
			for (int i = 0; i < particles.size(); i++) particles[i]->restDensity = restDensity;
			}, "%.3f kg/m³");

		EditorField::InputFloatScene(parent, "Viscosity", "##Viscosity", &viscosity, [&] {
			if (viscosity <= 0) viscosity = 0.0f;
			for (int i = 0; i < particles.size(); i++) particles[i]->viscosity = viscosity;
			});

		EditorField::InputFloatScene(parent, "Vorticity Strength", "##Vorticity Strength", &vorticityStrength, [&] {
			if (vorticityStrength < 0) vorticityStrength = 0.0f;
			for (int i = 0; i < particles.size(); i++) particles[i]->vorticityEps = vorticityStrength;
			});

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Equation of State", ImGuiTreeNodeFlags_DefaultOpen)) {
		EditorField::InputFloatScene(parent, "Stiffness", "##Stiffness", &stiffness, [&] {
			stiffness = std::max(0.0001f, stiffness);
			for (int i = 0; i < particles.size(); i++) particles[i]->stiffness = stiffness;
			});

		EditorField::InputFloatScene(parent, "Gamma", "##Gamma", &gamma, [&] {
			gamma = std::max(0.0001f, gamma);
			for (int i = 0; i < particles.size(); i++) particles[i]->gamma = gamma;
			});

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Thermal", ImGuiTreeNodeFlags_DefaultOpen)) {
		EditorField::InputFloatScene(parent, "Initial Temperature", "##InitialTemperature", &initialTemperature, [&] {
			SeedParticles();
			ResizeInstanceBuffer();
			EngineManager::getInstance().SceneChangeEvent();
			});

		EditorField::InputFloatScene(parent, "Ambient Temperature", "##AmbientTemperature", &ambientTemperature, [&] {
			for (int i = 0; i < particles.size(); i++) particles[i]->ambientTemperature = ambientTemperature;
			});

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Lifecycle", ImGuiTreeNodeFlags_DefaultOpen)) {
		EditorField::InputFloatScene(parent, "Dissipation Rate", "##DissipationRate", &dissipationRate, [&] {
			dissipationRate = std::max(0.0f, dissipationRate);
			for (int i = 0; i < particles.size(); i++) particles[i]->dissipationRate = dissipationRate;
			});

		ImGui::TreePop();
	}
}

void GasComponent::OnDelete() {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc && setShapeCallbackID != -1) rc->RemoveOnShapeSetCallback(setShapeCallbackID);

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc && transformCallbackID != -1) tc->RemoveTransformCallback(transformCallbackID);

	ClearParticles();
	isActive = false;

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

void GasComponent::CopyTo(Object* other) {
	GasComponent* target = other->GetComponent<GasComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<GasComponent>(other));
		target = other->GetComponent<GasComponent>();
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
	target->stiffness = stiffness;
	target->gamma = gamma;
	target->initialTemperature = initialTemperature;
	target->ambientTemperature = ambientTemperature;
	target->dissipationRate = dissipationRate;
	target->SeedParticles();
	target->ResizeInstanceBuffer();
	target->RebuildQuadGeometry();
	target->RebuildDensityQuadGeometry();
	target->SetEnabled(Enabled);
}

void GasComponent::Serialize(BinaryWriter& w) {
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
	w.Write(stiffness);
	w.Write(gamma);
	w.Write(initialTemperature);
	w.Write(ambientTemperature);
	w.Write(dissipationRate);
	w.Write(noiseScale);
	w.Write(noiseStrength);
	w.Write(riseSpeed);
}

void GasComponent::Deserialize(BinaryReader& r) {
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
	stiffness = r.Read<float>();
	gamma = r.Read<float>();
	initialTemperature = r.Read<float>();
	ambientTemperature = r.Read<float>();
	dissipationRate = r.Read<float>();
	noiseScale = r.Read<float>();
	noiseStrength = r.Read<float>();
	riseSpeed = r.Read<float>();
	SeedParticles();
	ResizeInstanceBuffer();
	RebuildQuadGeometry();
	RebuildDensityQuadGeometry();
}

void GasComponent::SetEnabled(bool enabled) {
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

void GasComponent::Draw() {
	if (!renderInitialized || particles.empty()) return;
	if (!Enabled) return;

	auto now = std::chrono::steady_clock::now();
	smokeTime += std::chrono::duration<float>(now - lastFrameTime).count();
	lastFrameTime = now;

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

void GasComponent::InitRenderResources() {
	unsigned int quadIdx[] = { 0, 1, 2,  2, 3, 0 };

	particleShader = Shader("Resources/Shaders/Fluid/fluid_vertex.txt", "Resources/Shaders/Fluid/fluid_fragment.txt");
	densityShader = Shader("Resources/Shaders/Gas/gas_density_vertex.txt", "Resources/Shaders/Gas/gas_density_fragment.txt");
	compositeShader = Shader("Resources/Shaders/Gas/gas_composite_vertex.txt", "Resources/Shaders/Gas/gas_composite_fragment.txt");
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

void GasComponent::RebuildQuadGeometry() {
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

void GasComponent::RebuildDensityQuadGeometry() {
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

void GasComponent::InitFullscreenQuad() {
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

void GasComponent::InitDensityFBO(int width, int height) {
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

void GasComponent::InitVectorFieldResources() {
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

void GasComponent::ResizeRenderTargets(int width, int height) {
	if (width <= 0 || height <= 0) return;
	if (densityInitialized && width == densityW && height == densityH) return;

	if (densityInitialized) {
		glDeleteFramebuffers(1, &densityFBO);
		glDeleteTextures(1, &densityTex);
		densityInitialized = false;
	}
	InitDensityFBO(width, height);
}

void GasComponent::DrawDensityPass() {
	if (!densityInitialized) return;

	GLint prevViewport[4];
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	GLint prevFBO = 0;                                  
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);     

	glBindFramebuffer(GL_FRAMEBUFFER, densityFBO);
	glViewport(0, 0, densityW, densityH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio,
		EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f); 
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

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void GasComponent::DrawComposite() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	compositeShader.use();
	compositeShader.setInt("densityTex", 0);
	compositeShader.setVec4D("coreColor", this->color);
	compositeShader.setVec4D("edgeColor", this->outlineColor);
	compositeShader.setFloat("threshold", metaballThreshold);
	compositeShader.setFloat("edgeSoft", metaballEdgeSoft);
	compositeShader.setVec2D("texelSize", glm::vec2(1.0f / densityW, 1.0f / densityH));
	compositeShader.setFloat("time", smokeTime);
	compositeShader.setFloat("noiseScale", noiseScale);
	compositeShader.setFloat("noiseStrength", noiseStrength);
	compositeShader.setFloat("riseSpeed", riseSpeed);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, densityTex);

	glBindVertexArray(fsQuadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec4 GasComponent::VelocityHeatmapColor(float t) {
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

void GasComponent::DrawVelocityField() {
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

void GasComponent::DrawParticlesDebug() {
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

void GasComponent::UpdateCollisionLayerMask() {
	CollisionComponent* cc = parent->GetComponent<CollisionComponent>();
	if (cc) {
		for (auto* p : particles) {
			p->collisionLayer = cc->collisionLayer;
			p->collisionMask = cc->collisionMask;
		}
	}
}

void GasComponent::UpdateInstanceBuffer() {
	if (!renderInitialized) return;

	std::vector<glm::vec3> positions;
	positions.reserve(particles.size());
	for (auto& p : particles) positions.push_back(p->position);

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec3), positions.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GasComponent::UpdateHeatBuffer() {
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

void GasComponent::UpdateParticleTransforms() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;
	if (localParticlePositions.size() != particles.size()) return;

	for (size_t i = 0; i < particles.size(); ++i) {
		glm::vec3 worldPos = tc->ProjectToWorld(localParticlePositions[i]);
		particles[i]->position = worldPos;
		particles[i]->predictedPosition = worldPos;
	}
}

void GasComponent::ResizeInstanceBuffer() {
	if (!renderInitialized) return;
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, heatVBO);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}