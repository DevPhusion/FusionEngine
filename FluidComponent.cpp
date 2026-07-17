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
		p->parent = parent;
		p->position = worldPos;
		p->predictedPosition = worldPos;
		p->prevPosition = worldPos;
		p->velocity = glm::vec3(0.0f);
		p->mass = 1.0f;
		p->invMass = 1 / p->mass;
		p->fluidPressure = pressure;
		p->lambda = 0.0f;
		p->vorticity = glm::vec3(0.0f);
		p->epsilon = epsilon;
		p->smoothingRadius = smoothingRadius;
		p->poly6Coeff = PhysicsEngine::getInstance().Poly6Coefficient(smoothingRadius);
		p->spikyCoeff = PhysicsEngine::getInstance().SpikyCoefficient(smoothingRadius);
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
		RebuildQuadGeometry();
		RebuildDensityQuadGeometry();
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
		RebuildDensityQuadGeometry();
		EngineManager::getInstance().EngineChangeEvent();
	}

	ImGui::Text("Metaball Threshold");
	ImGui::SameLine();
	ImGui::InputFloat("##MetaballThreshold", &metaballThreshold);

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

	ImGui::Text("Pressure");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Pressure", &pressure, 0.0f, 0.0f, "%.3f N/m²")) {
		for (int i = 0; i < particles.size(); i++)
		{
			if (pressure <= 0) pressure = 0.01f;
			particles[i]->fluidPressure = pressure;
		}
	}
}

void FluidComponent::OnDelete() {
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
	target->outlineColor = outlineColor;
	target->particleRadius = particleRadius;
	target->particleMass = particleMass;
	target->pressure = pressure;
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
	comp->particleMass = particleMass;
	comp->pressure = pressure;
	comp->epsilon = epsilon;
	comp->smoothingRadius = smoothingRadius;
	comp->Enabled = false;
	return comp;
}

void FluidComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(desiredParticleCount);
	w.Write(color);
	w.Write(outlineColor);
	w.Write(particleRadius);
	w.Write(particleMass);
	w.Write(pressure);
	w.Write(epsilon);
	w.Write(smoothingRadius);
}

void FluidComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	desiredParticleCount = r.Read<int>();
	color = r.Read<glm::vec4>();
	outlineColor = r.Read<glm::vec4>();
	particleRadius = r.Read<float>();
	particleMass = r.Read<float>();
	pressure = r.Read<float>();
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
}

void FluidComponent::Draw() {
	if (!renderInitialized || particles.empty()) return;
	if (!Enabled) return;

	// Lazily size the density target to the current viewport the first time
	// we draw, in case ResizeRenderTargets() was never wired to a resize event.
	if (!densityInitialized) {
		GLint vp[4];
		glGetIntegerv(GL_VIEWPORT, vp);
		if (vp[2] > 0 && vp[3] > 0) {
			ResizeRenderTargets(vp[2], vp[3]);
		}
		if (!densityInitialized) return;
	}

	UpdateInstanceBuffer();

	DrawDensityPass();
	DrawComposite();
}

void FluidComponent::InitRenderResources() {
	unsigned int quadIdx[] = { 0, 1, 2,  2, 3, 0 };

	particleShader = Shader("fluid_vertex.txt", "fluid_fragment.txt");
	densityShader = Shader("fluid_density_vertex.txt", "fluid_density_fragment.txt");
	compositeShader = Shader("fluid_composite_vertex.txt", "fluid_composite_fragment.txt");

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

	// Second, larger instanced quad set (smoothingRadius-sized) used only for
	// splatting particles into the density field. Shares instanceVBO.
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

	// Bigger than particleRadius on purpose: the falloff kernel needs
	// overlapping influence between neighboring particles to read as one
	// continuous blob instead of separated dots.
	float h = smoothingRadius;
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
	// NDC quad covering the whole screen; UV goes 0..1 to match densityTex.
	float verts[] = {
		// pos          // uv
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

void FluidComponent::DrawDensityPass() {
	if (!densityInitialized) return;

	GLint prevViewport[4];
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	glBindFramebuffer(GL_FRAMEBUFFER, densityFBO);
	glViewport(0, 0, densityW, densityH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // additive: overlapping particles build up density

	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio, EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);

	densityShader.use();
	densityShader.setMat4D("projection", projection);
	densityShader.setMat4D("view", Camera::getInstance().viewMatrix);

	glBindVertexArray(densityQuadVAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, (GLsizei)particles.size());
	glBindVertexArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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