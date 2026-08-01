#include "../../../Header Files/Core/Physics/PhysicsEngine.h"
#include "../../../Header Files/Components/SoftBodyComponent.h"

void PhysicsEngine::Setup(std::vector<std::unique_ptr<Object>>* objects) {
	this->allObjects = objects;
	EngineManager::getInstance().AddPhysicsModeChangedEvent([this]() {PhysicsModeChangeEvent();});
}

void PhysicsEngine::PhysicsModeChangeEvent() {
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate && 
		EngineManager::getInstance().EnginePrevPhysicsMode == EngineManager::PhysicsMode::Stop) {
		ScriptManager::getInstance().RunAllScriptsStart();
	}
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Stop) {
		ScriptManager::getInstance().RunAllScriptsStop();
		rigidBoundaries.clear();
		softBoundaries.clear();
	}
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Pause) {
		rigidBoundaries.clear();
		softBoundaries.clear();
	}
}

void PhysicsEngine::ProcessPhysics(float delta) {
	ScriptManager::getInstance().RunAllScriptsLoad();

	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Pause || EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Stop) {
		return;
	}

	ScriptManager::getInstance().RunAllScriptsProcess(delta);
	ObjectManager::getInstance().FlushPendingObjects();
	ObjectManager::getInstance().FlushPendingRemovals();

	TIME_BLOCK("Physics");

	UnRegisterTemporaryXPBDConstraint();
	UnRegisterTemporaryConstraint();

	{
		TIME_BLOCK("Update Forces");
		for (int i = 0; i < ForceRegistrations.size(); i++)
		{
			ForceRegistrations[i].fg->updateForce(ForceRegistrations[i].object, delta);
		}
	}

	for (int i = 0; i < allObjects->size(); i++)
	{
		{
			TIME_BLOCK("Rigidbody integrate velocity");
			if ((*allObjects)[i]->HasComponent<RigidBodyComponent>()) {
				(*allObjects)[i]->GetComponent<RigidBodyComponent>()->IntegrateVelocities(delta);
			}
		}
		{
			TIME_BLOCK("Soft body sync point mass");
			if ((*allObjects)[i]->HasComponent<SoftBodyComponent>()) {
				(*allObjects)[i]->GetComponent<SoftBodyComponent>()->ProcessSoftBody(delta);
			}
		}
		if ((*allObjects)[i]->HasComponent<CollisionComponent>() && EngineManager::getInstance().EngineSettings.colorCollisions) {
			(*allObjects)[i]->GetComponent<RenderComponent>()->color = glm::vec4(1);
		}
	}

	{
		TIME_BLOCK("PBF Solver");
		ResolvePBF(delta);
	}

	{
		TIME_BLOCK("Collision");
		std::vector<PotentialContact> potentialContacts;
		potentialContacts.reserve(allObjects->size() * 4);
		{
			TIME_BLOCK("Broad phase");
			root.getPotentialContacts(potentialContacts);
		}
		if (!potentialContacts.empty()) {
			{
				TIME_BLOCK("Narrow phase");
				ResolveContacts(potentialContacts.data(), (unsigned)potentialContacts.size());
			}
		}
	}

	{
		TIME_BLOCK("PGS Solver");
		ResolvePGSConstraints(delta);
	}

	{
		TIME_BLOCK("XPBD Solver");
		ResolveXPBDConstraints(delta);
	}

	{
		TIME_BLOCK("Contact cache");
		UpdateContactCache();
	}


	for (int i = 0; i < allObjects->size(); i++)
	{
		{
			TIME_BLOCK("Rigidbody integrate position");
			if ((*allObjects)[i]->HasComponent<RigidBodyComponent>()) {
				(*allObjects)[i]->GetComponent<RigidBodyComponent>()->IntegratePositions(delta);
			}
		}
	}

	{
		TIME_BLOCK("Fracture physics");
		ProcessFractures();
	}
}

void PhysicsEngine::RegisterForce(Object* object, ForceGenerator* fg) {
	ForceRegistrations.push_back(ForceRegistration(object, fg));
	std::function<void(int)> Wrapper = [fg](int index) {fg->processDisplay(index);};
	std::shared_ptr<std::function<void(int)>> sharedFunc = std::make_shared<std::function<void(int)>>(Wrapper);
	fg->setDisplayFunc(sharedFunc);
	object->GetComponent<RigidBodyComponent>()->AddDisplayFunc(sharedFunc);
}

void PhysicsEngine::UnRegisterForce(Object* object, ForceGenerator* fg) {
	for (int i = 0; i < ForceRegistrations.size(); i++)
	{
		if (ForceRegistrations[i].fg == fg && ForceRegistrations[i].object == object) {
			ForceRegistrations.erase(ForceRegistrations.begin() + i);
			object->GetComponent<RigidBodyComponent>()->RemoveDisplayFunc(fg->displayFunc);
			fg->displayFunc = nullptr;
		}
	}
}

void PhysicsEngine::UnRegisterAllForce(Object* object) {
	for (auto it = ForceRegistrations.begin(); it != ForceRegistrations.end(); )
	{
		if (it->object == object) {
			object->GetComponent<RigidBodyComponent>()->RemoveDisplayFunc(it->fg->displayFunc);
			it->fg->displayFunc = nullptr;
			it = ForceRegistrations.erase(it);
		}
		else {
			++it;
		}
	}
}

void PhysicsEngine::ClearRegistry() {
	ForceRegistrations.clear();
}

// Collision

void PhysicsEngine::ResolveContacts(PotentialContact* contacts, unsigned numContacts) {
	allContactPoints = {};

	for (int i = 0; i < numContacts; i++)
	{
		Object* objA = contacts[i].obj[0];
		Object* objB = contacts[i].obj[1];

		if (objA == nullptr || objB == nullptr) continue;

		CollisionComponent* ccA = objA->GetComponent<CollisionComponent>();
		CollisionComponent* ccB = objB->GetComponent<CollisionComponent>();
		TransformComponent* tcA = objA->GetComponent<TransformComponent>();
		TransformComponent* tcB = objB->GetComponent<TransformComponent>();

		float rA = -1;
		float rB = -1;

		std::visit([&](auto&& s) {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, CircleShape>) {
				rA = s.radius;
			}
			}, ccA->currentShape);

		std::visit([&](auto&& s) {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, CircleShape>) {
				rB = s.radius;
			}
			}, ccB->currentShape);

		PhysicsBody bodyA = PhysicsBody();
		RigidBodyComponent* pcA = objA->GetComponent<RigidBodyComponent>();
		SoftBodyComponent* sbA = objA->GetComponent<SoftBodyComponent>();
		bodyA.obj = objA;

		if (tcA) {
			bodyA.position = &tcA->worldPosition;
			bodyA.prevPos = &tcA->prevPos;
			bodyA.transformMatrix = &tcA->WorldMatrix;
			bodyA.rotation = &tcA->rotation;
		}
		if (pcA) {
			bodyA.velocity = &pcA->velocity;
			bodyA.angularVelocity = &pcA->angularVelocity;
			bodyA.invInertia = &pcA->inverseInertia;
			bodyA.invMass = &pcA->inverseMass;
		}

		PhysicsBody bodyB = PhysicsBody();
		RigidBodyComponent* pcB = objB->GetComponent<RigidBodyComponent>();
		SoftBodyComponent* sbB = objB->GetComponent<SoftBodyComponent>();
		bodyB.obj = objB;

		if (tcB) {
			bodyB.position = &tcB->worldPosition;
			bodyB.prevPos = &tcB->prevPos;
			bodyB.transformMatrix = &tcB->WorldMatrix;
			bodyB.rotation = &tcB->rotation;
		}
		if (pcB) {
			bodyB.velocity = &pcB->velocity;
			bodyB.angularVelocity = &pcB->angularVelocity;
			bodyB.invInertia = &pcB->inverseInertia;
			bodyB.invMass = &pcB->inverseMass;
		}

		// Rigidbody & Rigidbody or Rigidbody & Staticbody
		bool collisionResult = false;
		if ((pcA || pcB) && (!sbA && !sbB)) {
			{
				TIME_BLOCK("Rigid body collision");
				if (rA > 0.0f && rB > 0.0f && tcA->size.x == tcA->size.y && tcB->size.x == tcB->size.y) {
					{
						TIME_BLOCK("Circle-Circle");
						collisionResult = ResolveCircleCircleContacts(bodyA, bodyB, rA * tcA->size.x, rB * tcB->size.x);
					}
				}
				else if ((rA > 0.0f && tcA->size.x == tcA->size.y) || (rB > 0.0f && tcB->size.x == tcB->size.y)) {
					{
						TIME_BLOCK("Circle-Polygon");
						PhysicsBody circle;
						float radius = 0.0f;
						if (rA > 0.0f) {
							circle = bodyA;
							radius = rA * tcA->size.x;
						}
						else {
							circle = bodyB;
							radius = rB * tcB->size.x;
						}
						PhysicsBody poly = (rA > 0.0f) ? bodyB : bodyA;

						collisionResult = ResolveCirclePolygonContacts(circle, poly, radius, poly.obj->GetComponent<CollisionComponent>()->edges);
					}
				}
				else {
					{
						TIME_BLOCK("Polygon-Polygon");
						collisionResult = ResolvePolygonPolygonContacts(bodyA, bodyB);
					}
				}
			}
		}
		//Soft body & Soft body
		if (sbA && sbB) {
			{
				TIME_BLOCK("Soft body collision");
				std::vector<SoftEdge> edgesA = sbA->GetEdgesFromMassAggregate();
				std::vector<SoftEdge> edgesB = sbB->GetEdgesFromMassAggregate();

				bool axisValid = false;
				glm::vec3 globalAxis = ComputeSoftSoftAxis(sbA, edgesA, sbB, edgesB, &axisValid);
				glm::vec3 axisForAPoints = -globalAxis;

				bool anyPointContact = false;

				{
					TIME_BLOCK("First pass");
					for (auto& pm : sbA->MassAggregate) {
						if (ResolveSoftPointSoftEdgeContacts(pm->body, pm.get(), sbB, edgesB, pm->pointRadius, axisValid ? &axisForAPoints : nullptr))
							anyPointContact = true;
					}
				}

				{
					TIME_BLOCK("Second pass");
					for (auto& pm : sbB->MassAggregate) {
						if (ResolveSoftPointSoftEdgeContacts(pm->body, pm.get(), sbA, edgesA, pm->pointRadius, axisValid ? &globalAxis : nullptr))
							anyPointContact = true;
					}
				}
			}
		}

		if (EngineManager::getInstance().EngineSettings.colorCollisions && collisionResult) {
			objA->GetComponent<RenderComponent>()->color = glm::vec4(0, 1, 0, 1);
			objB->GetComponent<RenderComponent>()->color = glm::vec4(0, 1, 0, 1);
		}
		else if (EngineManager::getInstance().EngineSettings.colorCollisions) {
			objA->GetComponent<RenderComponent>()->color = glm::vec4(1, 0, 0, 1);
			objB->GetComponent<RenderComponent>()->color = glm::vec4(1, 0, 0, 1);
		}
	}
}

FluidSoftContact PhysicsEngine::DetectFluidSoftContact(const glm::vec3& particlePos, float radius, const SoftBoundary& soft) {
	FluidSoftContact contact;
	const std::vector<SoftEdge>& edges = soft.worldEdges;
	if (edges.empty()) return contact;

	std::vector<glm::vec3> starts, ends;
	starts.reserve(edges.size());
	ends.reserve(edges.size());
	for (auto& se : edges) { starts.push_back(se.edge.start); ends.push_back(se.edge.end); }

	bool centerInside = PointInPolygon(particlePos, starts, ends);
	ClosestPointOnEdge best = GetClosestPointOnEdge(particlePos, starts, ends, soft.worldCenter);
	if (!best.found) return contact;

	if (centerInside) {
		contact.hit = true;
		contact.normal = best.normal;
		contact.penetration = glm::min(radius + best.dist, radius * 3.0f);
		contact.point = best.point;
		contact.edgeIdx = best.edgeIdx;
		contact.edgeT = best.edgeT;
		return contact;
	}

	if (best.dist < radius) {
		contact.hit = true;
		contact.normal = best.normal;
		contact.penetration = glm::min(radius - best.dist, radius * 3.0f);
		contact.point = best.point;
		contact.edgeIdx = best.edgeIdx;
		contact.edgeT = best.edgeT;
	}
	return contact;
}

template<typename BoundaryT, typename ContactT, typename DetectFn>
void PhysicsEngine::ResolveFluidBoundaryContactsGeneric(std::vector<FluidParticle*>& particles, std::vector<int>& indices,
	std::vector<BoundaryT>& boundaries, std::vector<ContactT>& outContacts, DetectFn detect) {

	if (outContacts.size() != particles.size())
		outContacts.resize(particles.size());

	if (particles.empty() || boundaries.empty()) {
		std::fill(outContacts.begin(), outContacts.end(), ContactT());
		return;
	}

	std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
		[&](int i) {
			FluidParticle* p = particles[i];
			ContactT best;
			best.penetration = -INFINITY;

			for (int s = 0; s < (int)boundaries.size(); s++) {
				if (boundaries[s].obj == p->parent) continue;
				if (!layerOverlap(p->collisionLayer, p->collisionMask,
					boundaries[s].collisionLayer, boundaries[s].collisionMask))
					continue;
				ContactT c = detect(p->predictedPosition, p->collisionRadius, boundaries[s], s);
				if (c.hit && c.penetration > best.penetration) {
					best = c;
				}
			}
			if (!best.hit) best.penetration = 0.0f;

			const float positionCorrectionFactor = 0.2f;
			if (best.hit) {
				p->predictedPosition += best.normal * (best.penetration * positionCorrectionFactor);
			}

			outContacts[i] = best;
		});
}

void PhysicsEngine::ResolveFluidSoftContacts(float dtSub) {
	ResolveFluidBoundaryContactsGeneric(allFluidParticles, particleIndices, softBoundaries, fluidSoftContacts,
		[&](const glm::vec3& pos, float radius, const SoftBoundary& sb, int idx) -> FluidSoftContact {
			FluidSoftContact c;
			if (!sb.valid) return c;
			c = DetectFluidSoftContact(pos, radius, sb);
			if (c.hit) c.softIndex = idx;
			return c;
		});
}

void PhysicsEngine::ResolveFluidSoftImpulses(float dtSub) {
	int contactIterations = 4;
	float beta = 0.2f;
	float slop = 0.0005f;
	float restitution = 0.0f;
	float pressureGain = 5.0f;
	float buoyancyDamping = 1.0f;
	const float maxBiasVelocity = 2.0f;   

	for (int iter = 0; iter < contactIterations; iter++) {
		for (int i = 0; i < (int)allFluidParticles.size(); i++) {
			const FluidSoftContact& c = fluidSoftContacts[i];
			if (!c.hit) continue;

			FluidParticle* p = allFluidParticles[i];
			SoftBoundary& soft = softBoundaries[c.softIndex];
			const SoftEdge& se = soft.worldEdges[c.edgeIdx];
			PointMass* pmA = soft.sb->MassAggregate[se.idxA].get();
			PointMass* pmB = soft.sb->MassAggregate[se.idxB].get();

			float w1 = c.edgeT;
			float w0 = 1.0f - w1;

			float invMassEdge = pmA->inverseMass * w0 * w0 + pmB->inverseMass * w1 * w1;
			float invMassSum = p->invMass + invMassEdge;
			if (invMassSum <= 1e-8f) continue;

			glm::vec3 edgeVel = pmA->velocity * w0 + pmB->velocity * w1;
			glm::vec3 vRel = p->velocity - edgeVel;
			float vn = glm::dot(vRel, c.normal);

			float bias = std::min((beta / dtSub) * std::max(0.0f, c.penetration - slop), maxBiasVelocity);

			float lambda = (-(1.0f + restitution) * vn + bias) / invMassSum;
			lambda = std::max(lambda, 0.0f);

			glm::vec3 impulse = lambda * c.normal;

			p->velocity += p->invMass * impulse;
			pmA->velocity -= pmA->inverseMass * w0 * impulse;
			pmB->velocity -= pmB->inverseMass * w1 * impulse;
		}
	}
}

FluidRigidContact PhysicsEngine::DetectFluidRigidContact(const glm::vec3& particlePos, float radius, const RigidBoundary& rigid) {
	FluidRigidContact contact;
	if (rigid.worldEdges.empty()) return contact;

	std::vector<glm::vec3> starts, ends;
	starts.reserve(rigid.worldEdges.size());
	ends.reserve(rigid.worldEdges.size());
	for (auto& e : rigid.worldEdges) { starts.push_back(e.start); ends.push_back(e.end); }

	bool centerInside = PointInPolygon(particlePos, starts, ends);
	ClosestPointOnEdge best = GetClosestPointOnEdge(particlePos, starts, ends, rigid.worldCenter);
	if (!best.found) return contact;

	if (centerInside) {
		contact.hit = true;
		contact.normal = best.normal;
		contact.penetration = glm::min(radius + best.dist, radius * 3.0f);
		contact.point = best.point;
		return contact;
	}

	if (best.dist < radius) {
		contact.hit = true;
		contact.normal = best.normal;
		contact.penetration = radius - best.dist;
		contact.point = best.point;
	}
	return contact;
}

void PhysicsEngine::ResolveFluidRigidContacts(float dtSub) {
	ResolveFluidBoundaryContactsGeneric(allFluidParticles, particleIndices, rigidBoundaries, fluidRigidContacts,
		[&](const glm::vec3& pos, float radius, const RigidBoundary& rb, int idx) -> FluidRigidContact {
			FluidRigidContact c = DetectFluidRigidContact(pos, radius, rb);
			if (c.hit) c.rigidIndex = idx;
			return c;
		});
}

void PhysicsEngine::ResolveFluidRigidImpulses(float dtSub) {
	int contactIterations = 4;
	float beta = 0.2f;
	float slop = 0.0005f;
	float restitution = 0.0f;
	const float maxBiasVelocity = 2.0f; 

	for (int iter = 0; iter < contactIterations; iter++) {
		for (int i = 0; i < (int)allFluidParticles.size(); i++) {
			const FluidRigidContact& c = fluidRigidContacts[i];
			if (!c.hit) continue;

			FluidParticle* p = allFluidParticles[i];
			RigidBoundary& rigid = rigidBoundaries[c.rigidIndex];
			RigidBodyComponent* rb = rigid.rb;

			float rbInvMass = rb ? rb->inverseMass : 0.0f;
			float invMassSum = p->invMass + rbInvMass;
			if (invMassSum <= 1e-8f) continue;

			glm::vec3 velAtContact = rb
				? rb->velocity + glm::vec3(-rb->angularVelocity * (c.point - rigid.worldCenter).y,
					rb->angularVelocity * (c.point - rigid.worldCenter).x, 0.0f)
				: glm::vec3(0.0f);

			glm::vec3 vRel = p->velocity - velAtContact;
			float vn = glm::dot(vRel, c.normal);

			float bias = std::min((beta / dtSub) * std::max(0.0f, c.penetration - slop), maxBiasVelocity);

			float lambda = (-(1.0f + restitution) * vn + bias) / invMassSum;
			lambda = std::max(lambda, 0.0f);

			glm::vec3 impulse = lambda * c.normal;

			p->velocity += p->invMass * impulse;
			if (rb) {
				glm::vec3 r = c.point - rigid.worldCenter;
				float angImpulse = r.x * impulse.y - r.y * impulse.x;
				rb->velocity -= rbInvMass * impulse;
				rb->angularVelocity -= rb->inverseInertia * angImpulse;
			}
		}
	}
}

bool PhysicsEngine::ResolveSoftPointSoftEdgeContacts(PhysicsBody pointBody, PointMass* pointMass,
	SoftBodyComponent* otherSb, const std::vector<SoftEdge>& otherEdges,
	float vertexRadius, const glm::vec3* forcedAxis) {
	glm::vec3 center = pointMass->worldPos;

	bool centerInside = PointInPolygon(center, otherEdges);

	float bestDist = INFINITY;
	int bestEdge = -1;
	float bestT = 0.0f;
	glm::vec3 bestPoint = glm::vec3(0);
	glm::vec3 bestNormal = glm::vec3(0);

	for (int e = 0; e < (int)otherEdges.size(); e++)
	{
		const Edge& edge = otherEdges[e].edge;
		glm::vec3 ab = edge.end - edge.start;
		float len = glm::length(ab);
		if (len < 1e-8f) continue;

		glm::vec3 ac = center - edge.start;
		glm::vec3 abNorm = ab / len;
		float t = glm::clamp(glm::dot(ac, abNorm), 0.0f, len);
		glm::vec3 closest = edge.start + abNorm * t;

		glm::vec3 edgeNormal = glm::normalize(glm::vec3(ab.y, -ab.x, 0.0f));
		glm::vec3 toOtherCenter = otherSb->CenterPM->worldPos - edge.start;
		if (glm::dot(edgeNormal, toOtherCenter) > 0.0f) edgeNormal = -edgeNormal;

		float dist = glm::length(center - closest);
		if (dist < bestDist) {
			bestDist = dist; bestPoint = closest; bestNormal = edgeNormal;
			bestEdge = e; bestT = t / len;
		}
	}

	if (bestEdge < 0) return false;

	bool isColliding = false;
	glm::vec3 contactNormal;
	float  penetration = 0.0f;

	if (centerInside) {
		if (forcedAxis != nullptr) {
			contactNormal = *forcedAxis;
			float pointProj = glm::dot(center, contactNormal);
			float maxOtherProj = -INFINITY;
			for (const auto& pm : otherSb->MassAggregate) {
				maxOtherProj = std::max(maxOtherProj, glm::dot(pm->worldPos, contactNormal));
			}
			penetration = vertexRadius + (maxOtherProj - pointProj);
		}
		else {
			contactNormal = bestNormal;
			penetration = vertexRadius + bestDist;
		}
		isColliding = true;
	}
	else if (bestDist < vertexRadius) {
		glm::vec3 dir = center - bestPoint;
		contactNormal = (glm::length(dir) > 1e-6f && glm::dot(glm::normalize(dir), bestNormal) > 0.5f)
			? glm::normalize(dir) : bestNormal;
		penetration = vertexRadius - bestDist;
		isColliding = true;
	}

	if (!isColliding) return false;

	int i0 = otherEdges[bestEdge].idxA;
	int i1 = otherEdges[bestEdge].idxB;
	PointMass* nearPM = (bestT < 0.5f) ? otherSb->MassAggregate[i0].get() : otherSb->MassAggregate[i1].get();

	ContactPoint cp;
	cp.point = bestPoint;
	cp.normal = contactNormal;
	cp.penetration = penetration;
	cp.id = ContactID();
	allContactPoints.push_back(cp);  

	XPBDContactConstraint* constraint = new XPBDContactConstraint(
		pointBody, nearPM->body,
		contactNormal, vertexRadius,     
		0.0001f,                            
		0.6f, 0.4f);                  
	RegisterXPBDConstraint(constraint);

	return true;
}

SoftRigidContact PhysicsEngine::DetectSoftRigidContact(const glm::vec3& pmPos, float radius, const RigidBoundary& rigid, const glm::vec3* forcedAxis) {
	SoftRigidContact contact;
	if (rigid.worldEdges.empty()) return contact;

	std::vector<glm::vec3> starts, ends;
	starts.reserve(rigid.worldEdges.size());
	ends.reserve(rigid.worldEdges.size());
	for (auto& e : rigid.worldEdges) { starts.push_back(e.start); ends.push_back(e.end); }

	bool centerInside = PointInPolygon(pmPos, starts, ends);
	ClosestPointOnEdge best = GetClosestPointOnEdge(pmPos, starts, ends, rigid.worldCenter);
	if (!best.found) return contact;

	if (centerInside) {
		contact.hit = true;
		if (forcedAxis != nullptr) {
			glm::vec3 axis = *forcedAxis;   
			float pointProj = glm::dot(pmPos, axis);
			float maxRigidProj = -INFINITY;
			for (auto& e : rigid.worldEdges) maxRigidProj = std::max(maxRigidProj, glm::dot(e.start, axis));
			contact.normal = axis;
			contact.penetration = radius + (maxRigidProj - pointProj);
			contact.point = pmPos - axis * (maxRigidProj - pointProj);
		}
		else {
			contact.normal = best.normal;
			contact.penetration = radius + best.dist;
			contact.point = best.point;
		}
		return contact;
	}

	if (best.dist < radius) {
		contact.hit = true;
		contact.normal = best.normal;
		contact.penetration = radius - best.dist;
		contact.point = best.point;
	}
	return contact;
}

void PhysicsEngine::ResolveSoftRigidContacts(float dtSub) {
	if (softRigidContacts.size() != allSoftBodyPointMasses.size())
		softRigidContacts.resize(allSoftBodyPointMasses.size());

	if (allSoftBodyPointMasses.empty() || rigidBoundaries.empty()) {
		std::fill(softRigidContacts.begin(), softRigidContacts.end(), SoftRigidContact());
		return;
	}

	std::unordered_map<SoftBodyComponent*, int> sbIndex;          
	for (int s = 0; s < (int)softBoundaries.size(); s++) sbIndex[softBoundaries[s].sb] = s;

	for (size_t i = 0; i < allSoftBodyPointMasses.size(); i++) {
		PointMass* pm = allSoftBodyPointMasses[i];
		if (!pm->sb->Enabled) { softRigidContacts[i] = SoftRigidContact(); continue; }

		auto it = sbIndex.find(pm->sb);                            
		int softIdx = (it != sbIndex.end()) ? it->second : -1;

		SoftRigidContact best; best.penetration = -INFINITY;
		for (int r = 0; r < (int)rigidBoundaries.size(); r++) {
			if (softIdx >= 0 && !layerOverlap(softBoundaries[softIdx].collisionLayer, softBoundaries[softIdx].collisionMask,
				rigidBoundaries[r].collisionLayer, rigidBoundaries[r].collisionMask))
				continue;

			const glm::vec3* axisPtr = (softIdx >= 0 && rigidSoftAxisValid[r][softIdx]) ? &rigidSoftAxis[r][softIdx] : nullptr;  // ADD
			SoftRigidContact c = DetectSoftRigidContact(pm->worldPos, pm->pointRadius, rigidBoundaries[r], axisPtr);
			if (c.hit && c.penetration > best.penetration) { c.rigidIndex = r; best = c; }
		}
		if (!best.hit) best.penetration = 0.0f;
		softRigidContacts[i] = best;
	}
}

void PhysicsEngine::ApplySoftRigidPositionCorrection() {
	const float positionCorrectionFactor = 0.2f;
	for (size_t i = 0; i < allSoftBodyPointMasses.size(); i++) {
		SoftRigidContact& c = softRigidContacts[i];
		if (!c.hit) continue;

		PointMass* pm = allSoftBodyPointMasses[i];
		pm->worldPos += c.normal * (c.penetration * positionCorrectionFactor);
	}
}

void PhysicsEngine::ResolveSoftRigidImpulses(float dtSub) {
	int contactIterations = 4;
	float beta = 0.2f;
	float slop = 0.0005f;
	float restitution = 0.2f;
	const float maxBiasVelocity = 2.0f;
	const float staticFriction = 0.4f;
	const float dynamicFriction = 0.6f;


	for (auto& c : softRigidContacts) {
		c.accumNormalImpulse = 0.0f;
		c.accumTangentImpulse = 0.0f;
	}

	for (int iter = 0; iter < contactIterations; iter++) {
		for (size_t i = 0; i < allSoftBodyPointMasses.size(); i++) {
			SoftRigidContact& c = softRigidContacts[i];
			if (!c.hit) continue;

			PointMass* pm = allSoftBodyPointMasses[i];
			RigidBoundary& rigid = rigidBoundaries[c.rigidIndex];
			RigidBodyComponent* rb = rigid.rb;

			float rbInvMass = rb ? rb->inverseMass : 0.0f;
			float rbInvInertia = rb ? rb->inverseInertia : 0.0f;

			glm::vec3 r = c.point - rigid.worldCenter;

			float rn = r.x * c.normal.y - r.y * c.normal.x;
			float invMassSumN = pm->inverseMass + rbInvMass + rbInvInertia * rn * rn;
			if (invMassSumN <= 1e-8f) continue;

			auto velAtContact = [&]() {
				return rb
					? rb->velocity + glm::vec3(-rb->angularVelocity * (c.point - rigid.worldCenter).y,
						rb->angularVelocity * (c.point - rigid.worldCenter).x, 0.0f)
					: glm::vec3(0.0f);
				};

			glm::vec3 vRel = pm->velocity - velAtContact();
			float vn = glm::dot(vRel, c.normal);

			float bias = std::min((beta / dtSub) * std::max(0.0f, c.penetration - slop), maxBiasVelocity);

			float deltaLambda_n = (-(1.0f + restitution) * vn + bias) / invMassSumN;
			float newAccumNormal = std::max(c.accumNormalImpulse + deltaLambda_n, 0.0f);
			deltaLambda_n = newAccumNormal - c.accumNormalImpulse;
			c.accumNormalImpulse = newAccumNormal;

			glm::vec3 normalImpulse = deltaLambda_n * c.normal;

			pm->velocity += pm->inverseMass * normalImpulse;
			if (rb) {
				float angImpulse = r.x * normalImpulse.y - r.y * normalImpulse.x;
				rb->velocity -= rbInvMass * normalImpulse;
				rb->angularVelocity -= rbInvInertia * angImpulse;
			}

			glm::vec3 tangent = glm::vec3(-c.normal.y, c.normal.x, 0.0f);
			float rt = r.x * tangent.y - r.y * tangent.x;
			float invMassSumT = pm->inverseMass + rbInvMass + rbInvInertia * rt * rt;
			if (invMassSumT <= 1e-8f) continue;

			glm::vec3 vRelT = pm->velocity - velAtContact();
			float vt = glm::dot(vRelT, tangent);

			float deltaLambda_t = -vt / invMassSumT;
			float newAccumTangent = c.accumTangentImpulse + deltaLambda_t;

			float maxStatic = staticFriction * c.accumNormalImpulse;
			float clamped;
			if (std::abs(newAccumTangent) <= maxStatic) {
				clamped = newAccumTangent;
			}
			else {
				float maxDynamic = dynamicFriction * c.accumNormalImpulse;
				clamped = glm::clamp(newAccumTangent, -maxDynamic, maxDynamic);
			}
			deltaLambda_t = clamped - c.accumTangentImpulse;
			c.accumTangentImpulse = clamped;
			glm::vec3 tangentImpulse = deltaLambda_t * tangent;

			pm->velocity += pm->inverseMass * tangentImpulse;
			if (rb) {
				float angImpulseT = r.x * tangentImpulse.y - r.y * tangentImpulse.x;
				rb->velocity -= rbInvMass * tangentImpulse;
				rb->angularVelocity -= rbInvInertia * angImpulseT;
			}
		}
	}
}

RigidSoftContact PhysicsEngine::DetectRigidSoftContact(const glm::vec3& vertexPos, float radius, const SoftBoundary& soft, const glm::vec3* forcedAxis) {
	RigidSoftContact contact;
	const std::vector<SoftEdge>& edges = soft.worldEdges;
	if (edges.empty()) return contact;

	std::vector<glm::vec3> starts, ends;
	starts.reserve(edges.size());
	ends.reserve(edges.size());
	for (auto& se : edges) { starts.push_back(se.edge.start); ends.push_back(se.edge.end); }

	bool centerInside = PointInPolygon(vertexPos, starts, ends);
	ClosestPointOnEdge best = GetClosestPointOnEdge(vertexPos, starts, ends, soft.worldCenter);
	if (!best.found) return contact;

	if (centerInside) {
		contact.hit = true;
		contact.edgeIdx = best.edgeIdx;
		contact.edgeT = best.edgeT;

		if (forcedAxis != nullptr) {
			glm::vec3 axis = -(*forcedAxis);  
			float pointProj = glm::dot(vertexPos, axis);
			float maxSoftProj = -INFINITY;
			for (auto& pm : soft.sb->MassAggregate) maxSoftProj = std::max(maxSoftProj, glm::dot(pm->worldPos, axis));
			contact.normal = axis;
			contact.penetration = radius + (maxSoftProj - pointProj);
			contact.point = vertexPos - axis * (maxSoftProj - pointProj);
		}
		else {
			contact.normal = best.normal;
			contact.penetration = radius + best.dist;
			contact.point = best.point;
		}
		return contact;
	}

	if (best.dist < radius) {
		contact.hit = true;
		contact.normal = best.normal;
		contact.penetration = radius - best.dist;
		contact.point = best.point;
		contact.edgeIdx = best.edgeIdx;
		contact.edgeT = best.edgeT;
	}
	return contact;
}
void PhysicsEngine::ResolveRigidSoftContacts(float dtSub) {
	const float rigidVertexRadius = 0.01f;   

	if (rigidSoftContacts.size() != rigidVertices.size())
		rigidSoftContacts.resize(rigidVertices.size());

	if (rigidVertices.empty() || softBoundaries.empty()) {
		std::fill(rigidSoftContacts.begin(), rigidSoftContacts.end(), RigidSoftContact());
		return;
	}

	for (size_t i = 0; i < rigidVertices.size(); i++) {
		const RigidVertex& rv = rigidVertices[i];

		RigidSoftContact best; best.penetration = -INFINITY;
		for (int s = 0; s < (int)softBoundaries.size(); s++) {
			if (!softBoundaries[s].valid) continue;
			if (!layerOverlap(rigidBoundaries[rv.rigidIndex].collisionLayer, rigidBoundaries[rv.rigidIndex].collisionMask,
				softBoundaries[s].collisionLayer, softBoundaries[s].collisionMask))
				continue;
			const glm::vec3* axisPtr = rigidSoftAxisValid[rv.rigidIndex][s] ? &rigidSoftAxis[rv.rigidIndex][s] : nullptr; 
			RigidSoftContact c = DetectRigidSoftContact(rv.worldPos, rigidVertexRadius, softBoundaries[s], axisPtr);
			if (c.hit && c.penetration > best.penetration) { c.softIndex = s; best = c; }
		}
		if (!best.hit) best.penetration = 0.0f;
		rigidSoftContacts[i] = best;
	}
}

void PhysicsEngine::ResolveRigidSoftImpulses(float dtSub) {
	int contactIterations = 4;
	float beta = 0.2f;
	float slop = 0.0005f;
	float restitution = 0.0f;
	const float maxBiasVelocity = 2.0f;
	const float staticFriction = 0.4f;
	const float dynamicFriction = 0.6f;

	for (auto& c : rigidSoftContacts) {
		c.accumNormalImpulse = 0.0f;
		c.accumTangentImpulse = 0.0f;
	}

	for (int iter = 0; iter < contactIterations; iter++) {
		for (size_t i = 0; i < rigidVertices.size(); i++) {
			RigidSoftContact& c = rigidSoftContacts[i];
			if (!c.hit) continue;

			const RigidVertex& rv = rigidVertices[i];
			RigidBoundary& rigid = rigidBoundaries[rv.rigidIndex];
			RigidBodyComponent* rb = rigid.rb;

			SoftBoundary& soft = softBoundaries[c.softIndex];
			const SoftEdge& se = soft.worldEdges[c.edgeIdx];
			PointMass* pmA = soft.sb->MassAggregate[se.idxA].get();
			PointMass* pmB = soft.sb->MassAggregate[se.idxB].get();

			float w1 = c.edgeT;
			float w0 = 1.0f - w1;

			float rbInvMass = rb ? rb->inverseMass : 0.0f;
			float rbInvInertia = rb ? rb->inverseInertia : 0.0f;

			glm::vec3 r = rv.worldPos - rigid.worldCenter;   
			float rn = r.x * c.normal.y - r.y * c.normal.x;

			float invMassEdge = pmA->inverseMass * w0 * w0 + pmB->inverseMass * w1 * w1;
			float invMassSumN = rbInvMass + rbInvInertia * rn * rn + invMassEdge;
			if (invMassSumN <= 1e-8f) continue;

			auto velAtVertex = [&]() {
				return rb
					? rb->velocity + glm::vec3(-rb->angularVelocity * r.y, rb->angularVelocity * r.x, 0.0f)
					: glm::vec3(0.0f);
				};

			glm::vec3 edgeVel = pmA->velocity * w0 + pmB->velocity * w1;
			glm::vec3 vRel = velAtVertex() - edgeVel;
			float vn = glm::dot(vRel, c.normal);

			float bias = std::min((beta / dtSub) * std::max(0.0f, c.penetration - slop), maxBiasVelocity);

			float deltaLambda_n = (-(1.0f + restitution) * vn + bias) / invMassSumN;
			float newAccumNormal = std::max(c.accumNormalImpulse + deltaLambda_n, 0.0f);
			deltaLambda_n = newAccumNormal - c.accumNormalImpulse;
			c.accumNormalImpulse = newAccumNormal;

			glm::vec3 normalImpulse = deltaLambda_n * c.normal;

			if (rb) {
				rb->velocity += rbInvMass * normalImpulse;
				rb->angularVelocity += rbInvInertia * (r.x * normalImpulse.y - r.y * normalImpulse.x);
			}
			pmA->velocity -= pmA->inverseMass * w0 * normalImpulse;
			pmB->velocity -= pmB->inverseMass * w1 * normalImpulse;

			// Friction
			glm::vec3 tangent = glm::vec3(-c.normal.y, c.normal.x, 0.0f);
			float rt = r.x * tangent.y - r.y * tangent.x;
			float invMassSumT = rbInvMass + rbInvInertia * rt * rt + invMassEdge;
			if (invMassSumT <= 1e-8f) continue;

			glm::vec3 edgeVelT = pmA->velocity * w0 + pmB->velocity * w1;
			glm::vec3 vRelT = velAtVertex() - edgeVelT;
			float vt = glm::dot(vRelT, tangent);

			float deltaLambda_t = -vt / invMassSumT;
			float newAccumTangent = c.accumTangentImpulse + deltaLambda_t;

			float maxStatic = staticFriction * c.accumNormalImpulse;
			float clamped;
			if (std::abs(newAccumTangent) <= maxStatic) clamped = newAccumTangent;
			else {
				float maxDynamic = dynamicFriction * c.accumNormalImpulse;
				clamped = glm::clamp(newAccumTangent, -maxDynamic, maxDynamic);
			}
			deltaLambda_t = clamped - c.accumTangentImpulse;
			c.accumTangentImpulse = clamped;

			glm::vec3 tangentImpulse = deltaLambda_t * tangent;

			if (rb) {
				rb->velocity += rbInvMass * tangentImpulse;
				rb->angularVelocity += rbInvInertia * (r.x * tangentImpulse.y - r.y * tangentImpulse.x);
			}
			pmA->velocity -= pmA->inverseMass * w0 * tangentImpulse;
			pmB->velocity -= pmB->inverseMass * w1 * tangentImpulse;
		}
	}
}

void PhysicsEngine::ApplyRigidSoftPositionCorrection() {
	const float positionCorrectionFactor = 0.2f;
	for (size_t i = 0; i < rigidVertices.size(); i++) {
		RigidSoftContact& c = rigidSoftContacts[i];
		if (!c.hit) continue;

		SoftBoundary& soft = softBoundaries[c.softIndex];
		const SoftEdge& se = soft.worldEdges[c.edgeIdx];
		PointMass* pmA = soft.sb->MassAggregate[se.idxA].get();
		PointMass* pmB = soft.sb->MassAggregate[se.idxB].get();

		float w1 = c.edgeT;
		float w0 = 1.0f - w1;

		glm::vec3 correction = c.normal * (c.penetration * positionCorrectionFactor);
		pmA->worldPos -= correction * w0;
		pmB->worldPos -= correction * w1;
	}
}

bool PhysicsEngine::ResolveCircleCircleContacts(PhysicsBody bodyA, PhysicsBody bodyB, float rA, float rB) {
	glm::vec3 d = *bodyA.position - *bodyB.position;
	if (d == glm::vec3(0.0f)) {
		d = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	float dist = glm::length(d);
	if (dist < rA + rB) {
		glm::vec3 normal = glm::normalize(d);
		float penetration = rA + rB - dist;
		glm::vec3 cPoint = *bodyA.position - normal * ((rA - penetration) / 2.0f);

		ContactPoint cp;
		cp.point = cPoint;
		cp.normal = normal;
		cp.penetration = penetration;
		cp.id = ContactID();
		allContactPoints.push_back(cp);

		ContactConstraint* constraint = new ContactConstraint(
			bodyA, bodyB, cPoint, cPoint, ContactID(), normal, penetration, 0.2f, 0.4f, 0.6f);
		RegisterPGSConstraint(constraint);

		return true;
	}
	
	return false;
}

bool PhysicsEngine::ResolveCirclePolygonContacts(PhysicsBody circle, PhysicsBody polygon, float radius, std::vector<Edge> edges, const glm::vec3* forcedAxis) {
	glm::vec3 center = (circle.position != nullptr) ? *circle.position : glm::vec3(0);

	std::vector<Edge> worldEdges;
	for (const auto& e : edges) {
		Edge we;
		we.start = glm::vec3(*polygon.transformMatrix * glm::vec4(e.start, 1.0f));
		we.end = glm::vec3(*polygon.transformMatrix * glm::vec4(e.end, 1.0f));
		worldEdges.push_back(we);
	}

	glm::vec3 polyCenter = (polygon.position != nullptr) ? *polygon.position : glm::vec3(0);
	bool     centerInside = true;
	float minFaceDist = -INFINITY;
	glm::vec3 minFaceNormal = glm::vec3(0.0f);

	for (const auto& edge : worldEdges)
	{
		glm::vec3 ab = edge.end - edge.start;
		float len = glm::length(ab);
		if (len < 1e-8f) continue;
		glm::vec3 abNorm = ab / len;

		glm::vec3 edgeNormal = glm::normalize(glm::vec3(abNorm.y, -abNorm.x, 0.0f));
		glm::vec3 toPolyCenter = polyCenter - edge.start;
		if (glm::dot(edgeNormal, toPolyCenter) > 0.0f) {
			edgeNormal = -edgeNormal;
		}

		float signedDist = glm::dot(edgeNormal, center - edge.start);

		if (signedDist > 0.0f) {
			centerInside = false;
		}

		if (signedDist > minFaceDist) {
			minFaceDist = signedDist;
			minFaceNormal = edgeNormal;
		}
	}

	float    bestDist = INFINITY;
	glm::vec3 bestPoint = glm::vec3(0.0f);
	glm::vec3 bestNormal = glm::vec3(0.0f);

	if (!centerInside) {
		for (const auto& edge : worldEdges) {
			glm::vec3 ab = edge.end - edge.start;
			glm::vec3 ac = center - edge.start;
			float     len = glm::length(ab);

			glm::vec3 abNorm = ab / len;
			float     t = glm::clamp(glm::dot(ac, abNorm), 0.0f, len);
			glm::vec3 closest = edge.start + abNorm * t;

			glm::vec3 edgeNormal = glm::normalize(glm::vec3(ab.y, -ab.x, 0.0f));

			glm::vec3 toPolyCenter = polyCenter - edge.start;
			if (glm::dot(edgeNormal, toPolyCenter) > 0.0f) {
				edgeNormal = -edgeNormal;
			}

			if (glm::dot(edgeNormal, ac) > 0.0f) {
				centerInside = false;
			}

			float dist = glm::length(center - closest);
			if (dist < bestDist) {
				bestDist = dist;
				bestPoint = closest;
				bestNormal = edgeNormal;
			}
		}
	}


	bool isColliding = false;
	glm::vec3 contactNormal;
	float     penetration;
	glm::vec3 contactPoint = bestPoint;

	if (centerInside) {
		if (forcedAxis != nullptr) {
			contactNormal = *forcedAxis;
	
			float pointProj = glm::dot(center, contactNormal);
			float maxRigidProj = -INFINITY;
			for (const auto& edge : worldEdges) {
				maxRigidProj = std::max(maxRigidProj, glm::dot(edge.start, contactNormal));
			}
			penetration = radius + (maxRigidProj - pointProj);
			contactPoint = center - contactNormal * (maxRigidProj - pointProj);
		}
		else {
			contactNormal = minFaceNormal;
			penetration = radius - minFaceDist;
			contactPoint = center - contactNormal * minFaceDist;
		}
		isColliding = true;
	}
	else if (bestDist < radius) {
		glm::vec3 dir = center - bestPoint;
		if (glm::length(dir) > 1e-6f && glm::dot(glm::normalize(dir), bestNormal) > 0.5f) {
			contactNormal = glm::normalize(dir);
		}
		else {
			contactNormal = bestNormal;
		}
		contactPoint = bestPoint;
		penetration = radius - bestDist;
		isColliding = true;
	}

	if (isColliding) {
		ContactPoint cp;
		cp.point = contactPoint;
		cp.normal = contactNormal;
		cp.penetration = penetration;
		cp.id = ContactID();
		allContactPoints.push_back(cp);

		float cachedLambda = 0.0f;
		for (const auto& cached : contactsCache) {
			bool matchCircle = circle.obj ? (cached.objectA == circle.obj || cached.objectB == circle.obj)
				: (cached.pmA == circle.pm || cached.pmB == circle.pm);
			bool matchPolygon = (cached.objectA == polygon.obj || cached.objectB == polygon.obj);

			bool matchAB = (cached.objectA == polygon.obj && (circle.obj ? cached.objectB == circle.obj : cached.pmB == circle.pm)) ||
				(cached.objectB == polygon.obj && (circle.obj ? cached.objectA == circle.obj : cached.pmA == circle.pm));

			if (matchAB) {
				cachedLambda = cached.lambda;
				break;
			}
		}

		ContactConstraint* constraint = new ContactConstraint(
			circle, polygon,
			contactPoint, contactPoint,
			ContactID(), contactNormal, penetration,
			0.2f, 0.4f, 0.6f);
		constraint->SetInitialImpulse(cachedLambda);
		RegisterPGSConstraint(constraint);
	}
	
	return isColliding;
}

bool PhysicsEngine::ResolvePolygonPolygonContacts(PhysicsBody bodyA, PhysicsBody bodyB) {
	CollisionData colData = SAT(bodyA.obj, bodyB.obj);

	if (colData.isColliding) {
		std::vector<ContactPoint> points = GenerateContactPoints(colData);

		for (int j = 0; j < points.size(); j++)
		{
			allContactPoints.push_back(points[j]);

			float cachedLambda = 0.0f;
			for (const auto& cached : contactsCache) {
				if (cached.objectA == bodyA.obj && cached.objectB == bodyB.obj &&
					cached.id.referenceEdgeA == points[j].id.referenceEdgeA &&
					cached.id.incidentEdgeB == points[j].id.incidentEdgeB &&
					cached.id.vertexTypeA == points[j].id.vertexTypeA &&
					cached.id.vertexTypeB == points[j].id.vertexTypeB)
				{
					cachedLambda = cached.lambda;
					break;
				}
			}

			ContactConstraint* constraint = new ContactConstraint(bodyA, bodyB, points[j].point, points[j].point, points[j].id, points[j].normal, points[j].penetration, 0.2f, 0.4f, 0.6f);
			constraint->SetInitialImpulse(cachedLambda);
			RegisterPGSConstraint(constraint);
		}

	}
	
	return colData.isColliding;
}

std::vector<ContactPoint> PhysicsEngine::GenerateContactPoints(CollisionData collisionData) {
	std::vector<ContactPoint> ContactPoints = {};

	Edge referenceEdgeA = FindMostParallelEdge(collisionData.objAEdges, collisionData.normal);
	Edge referenceEdgeB = FindMostParallelEdge(collisionData.objBEdges, -collisionData.normal);
	Edge incidentEdgeA = FindMostAntiParallelEdge(collisionData.objAEdges, -collisionData.normal);
	Edge incidentEdgeB = FindMostAntiParallelEdge(collisionData.objBEdges, collisionData.normal);

	float scoreA = glm::dot(glm::vec3(glm::normalize(referenceEdgeA.end - referenceEdgeA.start).y, -glm::normalize(referenceEdgeA.end - referenceEdgeA.start).x, 0), collisionData.normal);
	float scoreB = glm::dot(glm::vec3(glm::normalize(referenceEdgeB.end - referenceEdgeB.start).y, -glm::normalize(referenceEdgeB.end - referenceEdgeB.start).x, 0), -collisionData.normal);

	Edge referenceEdge, incidentEdge;
	bool isA_Reference = true;
	int refEdgeIdx = 0;
	int incEdgeIdx = 0;

	if (scoreA > scoreB) {
		referenceEdge = referenceEdgeA;
		incidentEdge = incidentEdgeB;
		isA_Reference = true;
		for (size_t i = 0; i < collisionData.objAEdges.size(); ++i) if (collisionData.objAEdges[i].start == referenceEdge.start) refEdgeIdx = i;
		for (size_t i = 0; i < collisionData.objBEdges.size(); ++i) if (collisionData.objBEdges[i].start == incidentEdge.start) incEdgeIdx = i;
	}
	else {
		referenceEdge = referenceEdgeB;
		incidentEdge = incidentEdgeA;
		isA_Reference = false;
		for (size_t i = 0; i < collisionData.objBEdges.size(); ++i) if (collisionData.objBEdges[i].start == referenceEdge.start) refEdgeIdx = i;
		for (size_t i = 0; i < collisionData.objAEdges.size(); ++i) if (collisionData.objAEdges[i].start == incidentEdge.start) incEdgeIdx = i;
	}

	glm::vec3 v1 = referenceEdge.start;
	glm::vec3 v2 = referenceEdge.end;
	glm::vec3 tangent = glm::normalize(v2 - v1);

	ClipVertex incidentVertices[2];
	incidentVertices[0].position = incidentEdge.start;
	incidentVertices[1].position = incidentEdge.end;

	incidentVertices[0].id = { refEdgeIdx, incEdgeIdx, 0, 0 };
	incidentVertices[1].id = { refEdgeIdx, incEdgeIdx, 1, 0 };

	ClipVertex clipPoints1[2];
	glm::vec3 leftNormal = -tangent;
	float leftOffset = glm::dot(leftNormal, v1);
	int numPoints = ClipSegmentToLine(clipPoints1, incidentVertices, 2, leftNormal, leftOffset, refEdgeIdx, isA_Reference, 1);
	if (numPoints < 2) return ContactPoints;

	ClipVertex clipPoints2[2];
	glm::vec3 rightNormal = tangent;
	float rightOffset = glm::dot(rightNormal, v2);
	numPoints = ClipSegmentToLine(clipPoints2, clipPoints1, numPoints, rightNormal, rightOffset, refEdgeIdx, isA_Reference, 2);
	if (numPoints == 0) return ContactPoints;

	for (int i = 0; i < numPoints; i++) {
		float depth = 0.0f;

		if (isA_Reference) {
			depth = glm::dot(collisionData.normal, clipPoints2[i].position - referenceEdge.start);
		}
		else {
			depth = glm::dot(collisionData.normal, referenceEdge.start - clipPoints2[i].position);
		}

		if (depth >= 0.0f) {
			ContactPoint cp;
			cp.point = clipPoints2[i].position;
			cp.penetration = depth;
			cp.normal = collisionData.normal;
			cp.id = clipPoints2[i].id;
			ContactPoints.push_back(cp);
		}
	}

	return ContactPoints;
}

CollisionData PhysicsEngine::SAT(Object* objA, Object* objB) {
	CollisionComponent* ccA = objA->GetComponent<CollisionComponent>();
	TransformComponent* tcA = objA->GetComponent<TransformComponent>();
	CollisionComponent* ccB = objB->GetComponent<CollisionComponent>();
	TransformComponent* tcB = objB->GetComponent<TransformComponent>();

	std::vector<Edge> edgesA = ccA->edges;
	std::vector<Edge> edgesB = ccB->edges;

	std::vector<Edge> globalEdgesA;
	std::vector<Edge> globalEdgesB;

	std::vector<SeparatingAxis> Axes;
	std::vector<glm::vec3> vertsA;
	std::vector<glm::vec3> vertsB;

	for (int i = 0; i < edgesA.size(); i++)
	{
		glm::vec3 start = edgesA[i].start;
		glm::vec3 end = edgesA[i].end;

		SeparatingAxis axis = SeparatingAxis();
		axis.start = tcA->ProjectToWorld(start);
		axis.end = tcA->ProjectToWorld(end);

		Edge edge = Edge();
		edge.start = axis.start;
		edge.end = axis.end;
		globalEdgesA.push_back(edge);

		vertsA.push_back(axis.start);
		glm::vec3 tangent = axis.end - axis.start;
		axis.normal = glm::normalize(glm::vec3(tangent.y, -tangent.x, 0));
		Axes.push_back(axis);
	}

	for (int i = 0; i < edgesB.size(); i++)
	{
		glm::vec3 start = edgesB[i].start;
		glm::vec3 end = edgesB[i].end;

		SeparatingAxis axis = SeparatingAxis();
		axis.start = tcB->ProjectToWorld(start);
		axis.end = tcB->ProjectToWorld(end);

		Edge edge = Edge();
		edge.start = axis.start;
		edge.end = axis.end;
		globalEdgesB.push_back(edge);

		vertsB.push_back(axis.start);
		glm::vec3 tangent = axis.end - axis.start;
		axis.normal = glm::normalize(glm::vec3(tangent.y, -tangent.x, 0));
		Axes.push_back(axis);
	}

	CollisionData data = CollisionData();
	float minOverlap = std::numeric_limits<float>::max();
	glm::vec3 minAxisNormal = glm::vec3(0);

	for (int i = 0; i < Axes.size(); i++)
	{
		Projection projA = ProjectOntoAxis(vertsA, Axes[i]);
		Projection projB = ProjectOntoAxis(vertsB, Axes[i]);

		if (!projA.Overlaps(projB)) {
			data.isColliding = false;
			return data;
		}

		float overlap = std::min(projA.max, projB.max) - std::max(projA.min, projB.min);

		if (overlap < minOverlap) {
			minOverlap = overlap;
			minAxisNormal = Axes[i].normal;
		}
	}
	glm::vec3 centerA = tcA->GetWorldPosition();
	glm::vec3 centerB = tcB->GetWorldPosition();
	glm::vec3 dirAB = centerA - centerB;

	if (glm::dot(minAxisNormal, dirAB) < 0.0f) {
		minAxisNormal = -minAxisNormal;
	}

	data.isColliding = true;
	data.penetration = minOverlap;
	data.normal = minAxisNormal;

	if (ComputeSignedArea(vertsA) > 0.0f) {
		std::reverse(vertsA.begin(), vertsA.end());
		globalEdgesA.clear();
		for (int i = 0; i < vertsA.size(); i++) {
			Edge e;
			e.start = vertsA[i];
			e.end = vertsA[(i + 1) % vertsA.size()];
			globalEdgesA.push_back(e);
		}
	}

	if (ComputeSignedArea(vertsB) > 0.0f) {
		std::reverse(vertsB.begin(), vertsB.end());
		globalEdgesB.clear();
		for (int i = 0; i < vertsB.size(); i++) {
			Edge e;
			e.start = vertsB[i];
			e.end = vertsB[(i + 1) % vertsB.size()];
			globalEdgesB.push_back(e);
		}
	}

	data.objAEdges = globalEdgesA;
	data.objBEdges = globalEdgesB;

	return data;
}

void PhysicsEngine::GenerateRigidVertices() {
	rigidVertices.clear();
	for (int r = 0; r < (int)rigidBoundaries.size(); r++) {
		for (auto& e : rigidBoundaries[r].worldEdges) {
			RigidVertex v;
			v.rigidIndex = r;
			v.worldPos = e.start;
			rigidVertices.push_back(v);
		}
	}
}

void PhysicsEngine::GenerateRigidBoundaries() {
	rigidBoundaries.clear();
	for (auto& objPtr : *allObjects) {
		Object* obj = objPtr.get();
		if (obj->hideInHierarchy) continue;
		if (obj->HasComponent<SoftBodyComponent>() || obj->HasComponent<FluidComponent>()) continue;
		if (!obj->HasComponent<CollisionComponent>()) continue;

		CollisionComponent* cc = obj->GetComponent<CollisionComponent>();
		TransformComponent* tc = obj->GetComponent<TransformComponent>();
		if (!cc || !tc || cc->edges.empty()) continue;

		RigidBoundary b;
		b.obj = obj;
		b.rb = obj->GetComponent<RigidBodyComponent>();
		b.tc = tc;
		b.localEdges = cc->edges;
		b.collisionLayer = cc->collisionLayer;
		b.collisionMask = cc->collisionMask;
		rigidBoundaries.push_back(b);
	}
}

void PhysicsEngine::RefreshRigidBoundariesEdges() {
	for (auto& b : rigidBoundaries) {
		b.worldEdges.clear();
		b.worldEdges.reserve(b.localEdges.size());
		for (auto& e : b.localEdges) {
			Edge we;
			we.start = b.tc->ProjectToWorld(e.start);
			we.end = b.tc->ProjectToWorld(e.end);
			b.worldEdges.push_back(we);
		}
		b.worldCenter = b.tc->GetWorldPosition();

		std::vector<glm::vec3> pts;
		pts.reserve(b.worldEdges.size());
		for (auto& e : b.worldEdges) pts.push_back(e.start);
		b.totalArea = std::abs(ComputeSignedArea(pts));
	}
}

void PhysicsEngine::RefreshRigidBoundariesSurface() {
	for (auto& b : rigidBoundaries) {
		glm::vec3 rMin(INFINITY), rMax(-INFINITY);
		for (auto& e : b.worldEdges) {
			rMin = glm::min(rMin, glm::min(e.start, e.end));
			rMax = glm::max(rMax, glm::max(e.start, e.end));
		}
		b.surfaceValid = FindLocalFluidSurface(rMin, rMax, b.collisionLayer, b.collisionMask, b.surfaceY, b.rho0);
	}
}

void PhysicsEngine::GenerateSoftBoundaries() {
	softBoundaries.clear();
	for (auto& objPtr : *allObjects) {
		Object* obj = objPtr.get();
		if (obj->hideInHierarchy) continue;
		if (!obj->HasComponent<SoftBodyComponent>()) continue;

		SoftBodyComponent* sb = obj->GetComponent<SoftBodyComponent>();
		if (!sb->Enabled) continue;
		if (sb->MassAggregate.size() < 4) continue; // need edgeCount >= 3

		CollisionComponent* cc = obj->GetComponent<CollisionComponent>();

		SoftBoundary b;
		b.obj = obj;
		b.sb = sb;
		if (cc) {
			b.collisionLayer = cc->collisionLayer;
			b.collisionMask = cc->collisionMask;
		}
		softBoundaries.push_back(b);
	}
}

void PhysicsEngine::RefreshSoftBoundariesEdges() {
	for (auto& b : softBoundaries) {
		b.worldEdges = b.sb->GetEdgesFromMassAggregate();
		b.worldCenter = b.sb->CenterPM->worldPos;

		std::vector<glm::vec3> pts;
		for (auto& e : b.worldEdges) pts.push_back(e.edge.start);

		float area = std::abs(ComputeSignedArea(pts));
		b.valid = area > 1e-5f;
		b.totalArea = area;
	}
}

void PhysicsEngine::RefreshSoftBoundariesSurface() {
	for (auto& b : softBoundaries) {
		glm::vec3 sMin(INFINITY), sMax(-INFINITY);
		for (auto& e : b.worldEdges) {
			sMin = glm::min(sMin, glm::min(e.edge.start, e.edge.end));
			sMax = glm::max(sMax, glm::max(e.edge.start, e.edge.end));
		}
		b.surfaceValid = FindLocalFluidSurface(sMin, sMax, b.collisionLayer, b.collisionMask, b.surfaceY, b.rho0);
	}
}

void PhysicsEngine::ComputeFluidSurfaceQualification() {
	if (fluidSurfaceQualifies.size() != allFluidParticles.size())
		fluidSurfaceQualifies.resize(allFluidParticles.size());
	
	fluidBoundsMin = glm::vec3(INFINITY);
	fluidBoundsMax = glm::vec3(-INFINITY);
	for (auto* p : allFluidParticles) {
		glm::vec3 r(p->collisionRadius);
		fluidBoundsMin = glm::min(fluidBoundsMin, p->position - r);
		fluidBoundsMax = glm::max(fluidBoundsMax, p->position + r);
	}
	
	std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
		[&](int i) {
			FluidParticle * c = allFluidParticles[i];
			int neighborCount = 0;
			for (int j : fluidNeighbors[i]) {
				if (j == i) continue;
				if (glm::distance(allFluidParticles[j]->position, c->position) <= c->smoothingRadius) {
					neighborCount++;
					if (neighborCount >= buoyancyMinNeighbours) break;
					
				}
			}
			fluidSurfaceQualifies[i] = neighborCount >= buoyancyMinNeighbours;
		});
}

bool PhysicsEngine::ComputeSubmergedRegion(const std::vector<Edge>& worldEdges,
	float surfaceY, float& outArea, glm::vec3& outCentroid) {

	std::vector<glm::vec3> verts;
	verts.reserve(worldEdges.size());
	for (auto& e : worldEdges) verts.push_back(e.start);

	glm::vec3 normal(0.0f, 1.0f, 0.0f);
	std::vector<glm::vec3> submerged = ClipPolygonHalfPlane(verts, normal, surfaceY);

	if (submerged.size() < 3) { outArea = 0.0f; return false; }

	float area = ComputeSignedArea(submerged);
	glm::vec3 centroid(0.0f);
	for (auto& p : submerged) centroid += p;
	centroid /= (float)submerged.size();

	outArea = std::abs(area);
	outCentroid = centroid;
	return outArea > 1e-6f;
}

bool PhysicsEngine::PointInPolygon(const glm::vec3& p, const std::vector<glm::vec3>& starts, const std::vector<glm::vec3>& ends) {
	bool inside = false;
	for (size_t e = 0; e < starts.size(); e++) {
		const glm::vec3& p1 = starts[e];
		const glm::vec3& p2 = ends[e];
		if (((p1.y > p.y) != (p2.y > p.y)) &&
			(p.x < (p2.x - p1.x) * (p.y - p1.y) / (p2.y - p1.y) + p1.x)) {
			inside = !inside;
		}
	}
	return inside;
}

bool PhysicsEngine::PointInPolygon(const glm::vec3& p, const std::vector<Edge>& edges) {
	std::vector<glm::vec3> starts, ends;
	starts.reserve(edges.size());
	ends.reserve(edges.size());
	for (auto& e : edges) { starts.push_back(e.start); ends.push_back(e.end); }
	return PointInPolygon(p, starts, ends);
}

bool PhysicsEngine::PointInPolygon(const glm::vec3& p, const std::vector<SoftEdge>& edges) {
	std::vector<glm::vec3> starts, ends;
	starts.reserve(edges.size());
	ends.reserve(edges.size());
	for (auto& se : edges) { starts.push_back(se.edge.start); ends.push_back(se.edge.end); }
	return PointInPolygon(p, starts, ends);
}

bool PhysicsEngine::PointInPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon) {
	bool inside = false;
	int n = (int)polygon.size();
	for (int i = 0, j = n - 1; i < n; j = i++) {
		const glm::vec3& pi = polygon[i];
		const glm::vec3& pj = polygon[j];
		if (((pi.y > point.y) != (pj.y > point.y)) &&
			(point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x)) {
			inside = !inside;
		}
	}
	return inside;
}

ClosestPointOnEdge PhysicsEngine::GetClosestPointOnEdge(const glm::vec3& p, const std::vector<glm::vec3>& starts,
	const std::vector<glm::vec3>& ends, const glm::vec3& interiorRefPoint) {
	ClosestPointOnEdge r;
	for (size_t e = 0; e < starts.size(); e++) {
		glm::vec3 ab = ends[e] - starts[e];
		float len = glm::length(ab);
		if (len < 1e-8f) continue;

		glm::vec3 abNorm = ab / len;
		glm::vec3 ac = p - starts[e];
		float t = glm::clamp(glm::dot(ac, abNorm), 0.0f, len);
		glm::vec3 closest = starts[e] + abNorm * t;

		glm::vec3 edgeNormal = glm::normalize(glm::vec3(abNorm.y, -abNorm.x, 0.0f));
		glm::vec3 toRef = interiorRefPoint - starts[e];
		if (glm::dot(edgeNormal, toRef) > 0.0f) edgeNormal = -edgeNormal;

		float dist = glm::length(p - closest);
		if (dist < r.dist) {
			r.found = true;
			r.dist = dist; r.point = closest; r.normal = edgeNormal;
			r.edgeIdx = (int)e; r.edgeT = t / len;
		}
	}
	return r;
}

glm::vec3 PhysicsEngine::ComputeSoftSoftAxis(SoftBodyComponent* sbA, const std::vector<SoftEdge>& edgesA,
	SoftBodyComponent* sbB, const std::vector<SoftEdge>& edgesB, bool* outValid) {
	*outValid = false;

	std::vector<glm::vec3> axes;
	for (const auto& se : edgesA) {
		glm::vec3 tangent = se.edge.end - se.edge.start;
		float len = glm::length(tangent);
		if (len < 1e-8f) continue;
		axes.push_back(glm::normalize(glm::vec3(tangent.y, -tangent.x, 0.0f)));
	}
	for (const auto& se : edgesB) {
		glm::vec3 tangent = se.edge.end - se.edge.start;
		float len = glm::length(tangent);
		if (len < 1e-8f) continue;
		axes.push_back(glm::normalize(glm::vec3(tangent.y, -tangent.x, 0.0f)));
	}

	auto projectSb = [](SoftBodyComponent* sb, const glm::vec3& axis) -> std::pair<float, float> {
		float mn = INFINITY, mx = -INFINITY;
		for (const auto& pm : sb->MassAggregate) {
			float p = glm::dot(pm->worldPos, axis);
			mn = std::min(mn, p);
			mx = std::max(mx, p);
		}
		return { mn, mx };
		};

	bool valid = false;
	glm::vec3 bestAxis = FindMinOverlapAxis(axes,
		[&](const glm::vec3& axis) { return projectSb(sbA, axis); },
		[&](const glm::vec3& axis) { return projectSb(sbB, axis); },
		valid);
	if (!valid) return glm::vec3(0.0f);

	glm::vec3 centerA = sbA->CenterPM->worldPos;
	glm::vec3 centerB = sbB->CenterPM->worldPos;
	if (glm::dot(bestAxis, centerB - centerA) < 0.0f) bestAxis = -bestAxis;

	*outValid = true;
	return bestAxis;
}

glm::vec3 PhysicsEngine::ComputeRigidSoftAxis(const RigidBoundary& rigid, const SoftBoundary& soft, bool* outValid) {
	*outValid = false;

	std::vector<glm::vec3> axes;
	for (auto& e : rigid.worldEdges) {
		glm::vec3 tangent = e.end - e.start;
		float len = glm::length(tangent);
		if (len < 1e-8f) continue;
		axes.push_back(glm::normalize(glm::vec3(tangent.y, -tangent.x, 0.0f)));
	}
	for (auto& se : soft.worldEdges) {
		glm::vec3 tangent = se.edge.end - se.edge.start;
		float len = glm::length(tangent);
		if (len < 1e-8f) continue;
		axes.push_back(glm::normalize(glm::vec3(tangent.y, -tangent.x, 0.0f)));
	}

	auto projectRigid = [&](const glm::vec3& axis) -> std::pair<float, float> {
		float mn = INFINITY, mx = -INFINITY;
		for (auto& e : rigid.worldEdges) {
			float p = glm::dot(e.start, axis);
			mn = std::min(mn, p); mx = std::max(mx, p);
		}
		return { mn, mx };
		};
	auto projectSoft = [&](const glm::vec3& axis) -> std::pair<float, float> {
		float mn = INFINITY, mx = -INFINITY;
		for (auto& pm : soft.sb->MassAggregate) {
			float p = glm::dot(pm->worldPos, axis);
			mn = std::min(mn, p); mx = std::max(mx, p);
		}
		return { mn, mx };
		};

	bool valid = false;
	glm::vec3 bestAxis = FindMinOverlapAxis(axes, projectRigid, projectSoft, valid);
	if (!valid) return glm::vec3(0.0f);

	if (glm::dot(bestAxis, soft.worldCenter - rigid.worldCenter) < 0.0f) bestAxis = -bestAxis;

	*outValid = true;
	return bestAxis;
}

void PhysicsEngine::RefreshRigidSoftAxes() {
	rigidSoftAxis.assign(rigidBoundaries.size(), std::vector<glm::vec3>(softBoundaries.size(), glm::vec3(0)));
	rigidSoftAxisValid.assign(rigidBoundaries.size(), std::vector<bool>(softBoundaries.size(), false));

	for (int r = 0; r < (int)rigidBoundaries.size(); r++) {
		for (int s = 0; s < (int)softBoundaries.size(); s++) {
			if (!softBoundaries[s].valid) continue;
			bool valid = false;
			glm::vec3 axis = ComputeRigidSoftAxis(rigidBoundaries[r], softBoundaries[s], &valid);
			rigidSoftAxis[r][s] = axis;
			rigidSoftAxisValid[r][s] = valid;
		}
	}
}

float PhysicsEngine::ComputeSignedArea(const std::vector<glm::vec3>& vertices) {
	float area = 0.0f;
	int n = vertices.size();
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		area += vertices[i].x * vertices[j].y;
		area -= vertices[j].x * vertices[i].y;
	}
	return area * 0.5f;
}

template<typename ProjectFnA, typename ProjectFnB>
glm::vec3 PhysicsEngine::FindMinOverlapAxis(const std::vector<glm::vec3>& axes, ProjectFnA projectA, ProjectFnB projectB, bool& outValid) {
	outValid = false;
	float minOverlap = std::numeric_limits<float>::max();
	glm::vec3 bestAxis(0.0f);

	for (const auto& axis : axes) {
		auto [aMin, aMax] = projectA(axis);
		auto [bMin, bMax] = projectB(axis);

		if (aMax < bMin || bMax < aMin) return glm::vec3(0.0f); // separating axis found

		float overlap = std::min(aMax, bMax) - std::max(aMin, bMin);
		if (overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis = axis;
		}
	}
	outValid = true;
	return bestAxis;
}

Edge PhysicsEngine::FindMostParallelEdge(const std::vector<Edge>& edges, const glm::vec3& normal) {
	float maxDot = -INFINITY;
	Edge best;
	for (const auto& edge : edges) {
		glm::vec3 tangent = glm::normalize(edge.end - edge.start);
		glm::vec3 edgeNormal = glm::vec3(tangent.y, -tangent.x, 0.0f);
		float dot = glm::dot(edgeNormal, normal);
		if (dot > maxDot) { maxDot = dot; best = edge; }
	}
	return best;
}

Edge PhysicsEngine::FindMostAntiParallelEdge(const std::vector<Edge>& edges, const glm::vec3& normal) {
	float minDot = INFINITY;
	Edge best;
	for (const auto& edge : edges) {
		glm::vec3 tangent = glm::normalize(edge.end - edge.start);
		glm::vec3 edgeNormal = glm::vec3(tangent.y, -tangent.x, 0.0f);
		float dot = glm::dot(edgeNormal, normal);
		if (dot < minDot) { minDot = dot; best = edge; }
	}
	return best;
}

int PhysicsEngine::ClipSegmentToLine(ClipVertex vOut[2], const ClipVertex vIn[2], int numInPoints,
	const glm::vec3& normal, float offset, int referenceEdgeIndex, bool isA_Reference, int clipPlaneId) {
	int numOutPoints = 0;
	float d0 = glm::dot(normal, vIn[0].position) - offset;
	float d1 = glm::dot(normal, vIn[1].position) - offset;

	if (d0 <= 0.0f) vOut[numOutPoints++] = vIn[0];
	if (d1 <= 0.0f) vOut[numOutPoints++] = vIn[1];

	if ((d0 < 0.0f) != (d1 < 0.0f)) {
		float t = d0 / (d0 - d1);
		ClipVertex intersectionPoint;
		intersectionPoint.position = vIn[0].position + t * (vIn[1].position - vIn[0].position);

		intersectionPoint.id.referenceEdgeA = referenceEdgeIndex;
		intersectionPoint.id.incidentEdgeB = vIn[0].id.incidentEdgeB;
		intersectionPoint.id.vertexTypeA = vIn[0].id.vertexTypeA; 

		intersectionPoint.id.vertexTypeB = clipPlaneId;

		vOut[numOutPoints < 2 ? numOutPoints++ : 1] = intersectionPoint;
	}
	
	return numOutPoints;
}

Projection PhysicsEngine::ProjectOntoAxis(std::vector<glm::vec3>& vertices, SeparatingAxis axis) {
	float max = -INFINITY;
	float min = INFINITY;

	for (int i = 0; i < vertices.size(); i++)
	{
		float p = glm::dot(vertices[i], axis.normal);
		if (p > max) {
			max = p;
		}
		if (p < min) {
			min = p;
		}
	}

	Projection projection = Projection();
	projection.max = max;
	projection.min = min;
	return projection;
}

BAHNode<BoundingCircle>* PhysicsEngine::RegisterBoundingAreaNode(Object* obj, BoundingCircle boundingCircle) {
	if (root.obj == nullptr && root.children[0] == nullptr) {
		root.obj = obj;
		root.area = boundingCircle;
		for (int i = 0; i < allObjects->size(); i++)
		{
			if ((*allObjects)[i]->HasComponent<CollisionComponent>()) {
				(*allObjects)[i]->GetComponent<CollisionComponent>()->BAHnode = root.searchFor((*allObjects)[i].get());
			}
		}
		return &root;
	}
	else {
		BAHNode<BoundingCircle>* node = root.insert(obj, boundingCircle);

		for (int i = 0; i < allObjects->size(); i++)
		{
			if ((*allObjects)[i]->HasComponent<CollisionComponent>()) {
				(*allObjects)[i]->GetComponent<CollisionComponent>()->BAHnode = root.searchFor((*allObjects)[i].get());
			}
		}

		return node;
	}
}

void PhysicsEngine::UnRegisterBoundingAreaNode(Object* obj) {
	BAHNode<BoundingCircle>* node = root.searchFor(obj);
	if (node) {
		node->removeLeaf();
	}
}

// PGS Constraints

PhysicsBody PhysicsEngine::GetBodyFromObject(Object* obj) {
	PhysicsBody body;
	body.obj = obj;
	if (!obj) return body;

	if (auto* tc = obj->GetComponent<TransformComponent>()) {
		body.position = &tc->worldPosition;
		body.transformMatrix = &tc->WorldMatrix;
		body.rotation = &tc->rotation;
	}
	if (auto* sb = obj->GetComponent<SoftBodyComponent>()) {
		body.pm = &sb->CenterPM;
		body.position = &sb->CenterPM->worldPos;
		body.velocity = &sb->CenterPM->velocity;
		body.angularVelocity = &sb->CenterPM->angularVelocity;
		body.invInertia = &sb->CenterPM->InverseInertia;
		body.invMass = &sb->inverseMass;
	}
	if (auto* pc = obj->GetComponent<RigidBodyComponent>()) {
		body.velocity = &pc->velocity;
		body.angularVelocity = &pc->angularVelocity;
		body.invInertia = &pc->inverseInertia;
		body.invMass = &pc->inverseMass;
	}
	return body;
}

void PhysicsEngine::RegisterPGSConstraint(Constraint* constraint) {
	registeredPGSConstraints.push_back(constraint);
}

void PhysicsEngine::UpdateContactCache() {
	contactsCache.clear();
	for (auto* constraint : registeredPGSConstraints) {
		if (constraint->isTemporary) {
			auto* contact = static_cast<ContactConstraint*>(constraint);
			ContactCache entry;
			entry.objectA = contact->objectA.obj;
			entry.objectB = contact->objectB.obj;
			entry.pmA = contact->objectA.pm;   
			entry.pmB = contact->objectB.pm;  
			entry.id = contact->contactId;
			entry.lambda = contact->cacheLambda;
			contactsCache.push_back(entry);
		}
	}
}

void PhysicsEngine::UnRegisterTemporaryConstraint() {
	for (auto it = registeredPGSConstraints.begin(); it != registeredPGSConstraints.end(); ) {
		if ((*it)->isTemporary) {
			delete* it;
			it = registeredPGSConstraints.erase(it);
		}
		else {
			++it;
		}
	}
}

void PhysicsEngine::UnRegisterPGSConstraint(Constraint* constraint) {
	for (int i = 0; i < registeredPGSConstraints.size(); i++)
	{
		if (registeredPGSConstraints[i] == constraint) {
			registeredPGSConstraints[i]->Unregister();
			registeredPGSConstraints.erase(registeredPGSConstraints.begin() + i);
		}
	}
}

void PhysicsEngine::ResolvePGSConstraintsForSubstep(float dtSub, float frameDelta) {
	std::vector<SolverRow> solverRows;
	solverRows.reserve(registeredPGSConstraints.size() * 2);

	for (auto* constraint : registeredPGSConstraints) {
		float biasDelta = constraint->isTemporary ? frameDelta : dtSub;
		constraint->Prepare(solverRows, biasDelta);
	}

	if (solverRows.empty()) return;

	std::vector<int> sortedIndices(solverRows.size());
	std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
	std::stable_sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
		return solverRows[a].bias > solverRows[b].bias;
		});

	const int velocityIterations = 8;
	for (int i = 0; i < velocityIterations; i++) {
		for (int idx : sortedIndices) {
			auto& row = solverRows[idx];

			float relVel = 0.0f;
			if (row.objectA.velocity != nullptr && row.objectA.angularVelocity != nullptr) relVel += glm::dot(row.jacobian.linearA, *row.objectA.velocity)
				+ row.jacobian.angularA * *row.objectA.angularVelocity;
			if (row.objectB.velocity != nullptr && row.objectB.angularVelocity != nullptr) relVel += glm::dot(row.jacobian.linearB, *row.objectB.velocity)
				+ row.jacobian.angularB * *row.objectB.angularVelocity;

			float lambdaRaw = row.effectiveMass * (row.bias - relVel - row.softnessCFM * row.lambda);
			float lambdaOld = row.lambda;
			row.lambda += lambdaRaw;

			if (row.parentConstraint) {
				row.parentConstraint->PostIterationClamp(solverRows, idx, i);
			}

			float deltaLambda = row.lambda - lambdaOld;
			if (row.objectA.velocity != nullptr && row.objectA.angularVelocity != nullptr) {
				*row.objectA.velocity += *row.objectA.invMass * row.jacobian.linearA * deltaLambda;
				*row.objectA.angularVelocity += *row.objectA.invInertia * row.jacobian.angularA * deltaLambda;
			}
			if (row.objectB.velocity != nullptr && row.objectB.angularVelocity != nullptr) {
				*row.objectB.velocity += *row.objectB.invMass * row.jacobian.linearB * deltaLambda;
				*row.objectB.angularVelocity += *row.objectB.invInertia * row.jacobian.angularB * deltaLambda;
			}
		}
	}

	for (auto* constraint : registeredPGSConstraints) {
		constraint->PostSolve(solverRows);
	}
}

void PhysicsEngine::ResolvePGSConstraints(float delta) {
	std::vector<SolverRow> solverRows;
	solverRows.reserve(registeredPGSConstraints.size() * 3);

	for (auto* constraint : registeredPGSConstraints) {
		constraint->Prepare(solverRows, delta);
	}

	std::vector<int> sortedIndices(solverRows.size());
	std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
	std::stable_sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
		return solverRows[a].bias > solverRows[b].bias;
		});

	for (int idx : sortedIndices) {
		{
			TIME_BLOCK("Warm start");
			auto& row = solverRows[idx];
			if (!row.warmStart || row.lambda == 0.0f) continue;
			if (row.objectA.velocity != nullptr && row.objectA.angularVelocity != nullptr) {
				*row.objectA.velocity += *row.objectA.invMass * row.jacobian.linearA * row.lambda;
				*row.objectA.angularVelocity += *row.objectA.invInertia * row.jacobian.angularA * row.lambda;
			}
			if (row.objectB.velocity != nullptr && row.objectB.angularVelocity != nullptr) {
				*row.objectB.velocity += *row.objectB.invMass * row.jacobian.linearB * row.lambda;
				*row.objectB.angularVelocity += *row.objectB.invInertia * row.jacobian.angularB * row.lambda;
			}
		}
	}

	const int velocityIterations = 30;
	{
		TIME_BLOCK("PGS solve");
		for (int i = 0; i < velocityIterations; i++) {
			for (int idx : sortedIndices) {
				auto& row = solverRows[idx];
				float lambdaRaw = 0.0f;
				float lambdaOld = 0.0f;
				{
					TIME_BLOCK("Solve lambda");
					float relVel = 0.0f;
					if (row.objectA.velocity != nullptr && row.objectA.angularVelocity != nullptr) relVel += glm::dot(row.jacobian.linearA, *row.objectA.velocity)
						+ row.jacobian.angularA * *row.objectA.angularVelocity;
					if (row.objectB.velocity != nullptr && row.objectB.angularVelocity != nullptr) relVel += glm::dot(row.jacobian.linearB, *row.objectB.velocity)
						+ row.jacobian.angularB * *row.objectB.angularVelocity;

					lambdaRaw = row.effectiveMass * (row.bias - relVel - row.softnessCFM * row.lambda);
					lambdaOld = row.lambda;
					row.lambda += lambdaRaw;
				}
				
				{
					TIME_BLOCK("Post iteration clamp");

					if (row.parentConstraint) {
						row.parentConstraint->PostIterationClamp(solverRows, idx, i);
					}
				}

				{
					TIME_BLOCK("Solve velocity");

					float deltaLambda = row.lambda - lambdaOld;
					if (row.objectA.velocity != nullptr && row.objectA.angularVelocity != nullptr) {
						*row.objectA.velocity += *row.objectA.invMass * row.jacobian.linearA * deltaLambda;
						*row.objectA.angularVelocity += *row.objectA.invInertia * row.jacobian.angularA * deltaLambda;
					}
					if (row.objectB.velocity != nullptr && row.objectB.angularVelocity != nullptr) {
						*row.objectB.velocity += *row.objectB.invMass * row.jacobian.linearB * deltaLambda;
						*row.objectB.angularVelocity += *row.objectB.invInertia * row.jacobian.angularB * deltaLambda;
					}
				}
			}
		}
	}
	

	{
		TIME_BLOCK("Post solve");
		for (auto* constraint : registeredPGSConstraints) {
			constraint->PostSolve(solverRows);
		}
	}

	for (auto* constraint : registeredPGSConstraints)
	{
		if (!constraint->isTemporary) continue;
		auto* contact = static_cast<ContactConstraint*>(constraint);

		if (contact->objectA.obj && contact->objectB.obj) {
			{
				TIME_BLOCK("Fracture impulse check");
				float appliedImpulse = std::abs(contact->cacheLambda);
				Object* a = contact->objectA.obj;
				Object* b = contact->objectB.obj;
				if (a->HasComponent<FractureComponent>()) {
					FractureComponent* fc = a->GetComponent<FractureComponent>();
					if (fc->fracturable && appliedImpulse > fc->impulseThreshold)
						pendingFractures.push_back({ a, contact->attachPointA, appliedImpulse });
				}
				if (b->HasComponent<FractureComponent>()) {
					FractureComponent* fc = b->GetComponent<FractureComponent>();
					if (fc->fracturable && appliedImpulse > fc->impulseThreshold)
						pendingFractures.push_back({ b, contact->attachPointB, appliedImpulse });
				}
			}	
		}
	}
}

// XPBD constraints
void PhysicsEngine::RegisterXPBDConstraint(XPBDConstraint* constraint) {
	registeredXPBDConstraints.push_back(constraint);
}

void PhysicsEngine::UnRegisterXPBDConstraint(XPBDConstraint* constraint) {
	for (int i = 0; i < registeredXPBDConstraints.size(); i++)
	{
		if (registeredXPBDConstraints[i] == constraint) {
			registeredXPBDConstraints.erase(registeredXPBDConstraints.begin() + i);
		}
	}
}

void PhysicsEngine::UnRegisterTemporaryXPBDConstraint() {
	for (auto it = registeredXPBDConstraints.begin(); it != registeredXPBDConstraints.end(); ) {
		if ((*it)->isTemporary) {
			delete* it;
			it = registeredXPBDConstraints.erase(it);
		}
		else {
			++it;
		}
	}
}

void PhysicsEngine::ResolveXPBDConstraints(float delta) {
	int substeps = 8;
	float dtSub = delta / substeps;

	GenerateRigidBoundaries();       
	RefreshRigidBoundariesEdges();
	GenerateRigidVertices();     
	GenerateSoftBoundaries();

	for (int i = 0; i < substeps; i++) {
		{
			TIME_BLOCK("Apply gas pressure");
			for (int j = 0; j < allObjects->size(); j++)
			{
				SoftBodyComponent* sb = (*allObjects)[j]->GetComponent<SoftBodyComponent>();
				if (sb && sb->Enabled && sb->useGasPressure) sb->ApplyGasPressure();
			}
		}

		{
			TIME_BLOCK("Integrate point masses");
			for (auto& pm : allSoftBodyPointMasses) {
				if (!pm->sb->Enabled) continue;
				if (pm->sb->isDragging) pm->ProcessDragForce();

				pm->prevPos = pm->worldPos;
				pm->velocity += (pm->baseAcceleration + pm->acceleration) * dtSub;
				pm->worldPos += pm->velocity * dtSub;
				pm->acceleration = glm::vec3(0);
			}
		}
		
		{
			TIME_BLOCK("Integrate proxy");
			for (auto* proxy : allSoftBodyProxies) {
				if (!proxy) continue;
				proxy->prevPos = proxy->worldPos;
				proxy->prevRotation = proxy->rotation;
				proxy->worldPos += proxy->velocity * dtSub;
				proxy->rotation += proxy->angularVelocity * dtSub;
			}
		}

		for (auto* constraint : registeredXPBDConstraints) constraint->ResetLambda();

		const int posIterations = 4;
		for (int it = 0; it < posIterations; it++) {
			{
				TIME_BLOCK("Solve position");
				for (auto* c : registeredXPBDConstraints) {
					c->SolvePosition(dtSub);
				}
			}
		}

		{
			TIME_BLOCK("Refresh soft boundary edges");  
			RefreshSoftBoundariesEdges();
		}

		{
			TIME_BLOCK("Refresh rigid-soft axes");  
			RefreshRigidSoftAxes();
		}

		{
			TIME_BLOCK("Soft-Rigid contact");   
			ResolveSoftRigidContacts(dtSub);
		}

		{
			TIME_BLOCK("Rigid-Soft contact");  
			ResolveRigidSoftContacts(dtSub);
		}

		{
			TIME_BLOCK("Resolve PGS for sub step");
			ResolvePGSConstraintsForSubstep(dtSub, delta);
		}

		{
			TIME_BLOCK("Solve point mass velocity");
			for (auto& pm : allSoftBodyPointMasses) {
				if (!pm->sb->Enabled) continue;
				pm->velocity = (pm->worldPos - pm->prevPos) / dtSub;
			}
		}

		{
			TIME_BLOCK("Soft-Rigid impulse");   
			ResolveSoftRigidImpulses(dtSub);
		}

		{
			TIME_BLOCK("Rigid-Soft impulse");   
			ResolveRigidSoftImpulses(dtSub);
		}

		{
			TIME_BLOCK("Soft-Rigid position correction");   
			ApplySoftRigidPositionCorrection();
		}

		{
			TIME_BLOCK("Rigid-Soft position correction");   
			ApplyRigidSoftPositionCorrection();
		}

		{
			TIME_BLOCK("Solve proxy velocity");
			for (auto* proxy : allSoftBodyProxies) {
				if (!proxy) continue;
				proxy->velocity = (proxy->worldPos - proxy->prevPos) / dtSub;

				float dTheta = proxy->rotation - proxy->prevRotation;
				dTheta = atan2(sin(dTheta), cos(dTheta));
				proxy->angularVelocity = dTheta / dtSub;
			}
		}
	}
}

//PBF 

float PhysicsEngine::Poly6Coefficient(float h) {
	return 315.0f / (64.0f * (float)std::numbers::pi * std::powf(h, 9));
}
float PhysicsEngine::SpikyCoefficient(float h) {
	return -45.0f / ((float)std::numbers::pi * std::powf(h, 6));
}

float PhysicsEngine::Poly6Kernel(float poly6Coeff, float h2, float r2) {
	float term = h2 - r2;
	return poly6Coeff * (term * term * term);
}

glm::vec3 PhysicsEngine::SpikyGradientKernel(float spikyCoeff, float h, float r, glm::vec3 rVec) {
	if (r < 1e-6f) return glm::vec3(0.0f);
	float rSafe = std::max(r, 0.1f * h);
	float hr = h - r;
	return (spikyCoeff * hr * hr / rSafe) * (rVec / r);
}

void PhysicsEngine::SolvePBFLambda(int particleIdx, std::vector<int>& neighboursIdx) {
	FluidParticle* pi = allFluidParticles[particleIdx];
	float rho0 = pi->restDensity;
	float h = pi->smoothingRadius;
	float h2 = h * h;
	float poly6Coeff = pi->poly6Coeff;
	float spikyCoeff = pi->spikyCoeff;

	float restDensity = 0.0f;
	glm::vec3 gradSelf(0.0f);
	float sumGradSq = 0.0f;

	for (int j : neighboursIdx) {
		FluidParticle* pj = allFluidParticles[j];
		glm::vec3 rVec = pi->predictedPosition - pj->predictedPosition;
		float r2 = glm::length2(rVec);
		if (r2 > h2) continue;

		restDensity += pj->mass * Poly6Kernel(poly6Coeff, h2, r2);

		float r = std::sqrt(r2);
		glm::vec3 grad = SpikyGradientKernel(spikyCoeff, h, r, rVec) / rho0;
		sumGradSq += glm::dot(grad, grad);
		gradSelf += grad;
	}
	sumGradSq += glm::dot(gradSelf, gradSelf);

	float C = restDensity / rho0 - 1.0f;
	pi->density = restDensity;
	pi->lambda = -C / (sumGradSq + pi->epsilon);
}

void PhysicsEngine::SolvePBFPosition(int particleIdx, std::vector<int>& neighboursIdx, std::vector<glm::vec3>& outPositions) {
	FluidParticle* pi = allFluidParticles[particleIdx];
	float rho0 = pi->restDensity;
	float h = pi->smoothingRadius;
	float h2 = h * h;
	float poly6Coeff = pi->poly6Coeff;
	float spikyCoeff = pi->spikyCoeff;

	float deltaQ = 0.2f * h;
	float wq = Poly6Kernel(poly6Coeff, h2, deltaQ * deltaQ);
	bool wqValid = wq > 1e-8f;

	glm::vec3 deltaP(0.0f);
	for (int j : neighboursIdx) {
		FluidParticle* pj = allFluidParticles[j];
		glm::vec3 rVec = pi->predictedPosition - pj->predictedPosition;
		float r2 = rVec.x * rVec.x + rVec.y * rVec.y + rVec.z * rVec.z;
		if (r2 > h2) continue;

		float r = std::sqrt(r2);

		float sCorr = 0.0f;
		if (wqValid) {
			float w = Poly6Kernel(poly6Coeff, h2, r2);
			float x = w / wq;
			float x2 = x * x;
			sCorr = -0.1f * (x2 * x2);
		}

		glm::vec3 grad = SpikyGradientKernel(spikyCoeff, h, r, rVec);

		deltaP += (pi->lambda + pj->lambda + sCorr) * grad;
	}

	glm::vec3 result = pi->predictedPosition + deltaP / rho0;
	outPositions[particleIdx] = result;
}

void PhysicsEngine::SolveXSPHViscosity(int particleIdx, std::vector<int>& neighboursIdx, std::vector<glm::vec3>& outDeltas) {
	FluidParticle* pi = allFluidParticles[particleIdx];
	float h2 = pi->smoothingRadius * pi->smoothingRadius;
	float poly6Coeff = pi->poly6Coeff;

	glm::vec3 delta(0.0f);
	for (int j : neighboursIdx) {
		FluidParticle* pj = allFluidParticles[j];
		glm::vec3 rVec = pi->position - pj->position;
		float r2 = glm::dot(rVec, rVec);
		if (r2 > h2) continue;

		float w = Poly6Kernel(poly6Coeff, h2, r2);
		delta += w * (pj->velocity - pi->velocity);
	}
	outDeltas[particleIdx] = pi->viscosity * delta;
}

void PhysicsEngine::SolveVorticityConfinement(int particleIdx, std::vector<int>& neighboursIdx, std::vector<float>& omega, std::vector<glm::vec3>& outForce) {
	FluidParticle* pi = allFluidParticles[particleIdx];
	float h = pi->smoothingRadius;
	float spikyCoeff = pi->spikyCoeff;

	glm::vec3 eta(0.0f);
	for (int j : neighboursIdx) {
		FluidParticle* pj = allFluidParticles[j];
		glm::vec3 rVec = pi->position - pj->position;
		float r2 = glm::dot(rVec, rVec);
		if (r2 > h * h) continue;
		float r = std::sqrt(r2);
		if (r < 1e-6f) continue;

		glm::vec3 grad = SpikyGradientKernel(spikyCoeff, h, r, rVec);

		eta += std::abs(omega[j]) * grad;
	}

	float etaLen = glm::length(eta);
	if (etaLen < 1e-6f) { outForce[particleIdx] = glm::vec3(0.0f); return; }

	glm::vec3 N = eta / etaLen;
	glm::vec3 omegaVec = glm::vec3(0.0f, 0.0f, omega[particleIdx]);

	outForce[particleIdx] = pi->vorticityEps * glm::cross(N, omegaVec);
}

void PhysicsEngine::ComputeVorticity(int particleIdx, std::vector<int>& neighboursIdx, std::vector<float>& outOmegas) {
	FluidParticle* pi = allFluidParticles[particleIdx];
	float h = pi->smoothingRadius;
	float spikyCoeff = pi->spikyCoeff;

	float omega = 0.0f;
	for (int j : neighboursIdx)
	{
		FluidParticle* pj = allFluidParticles[j];
		glm::vec3 rVec = pi->position - pj->position;
		float r2 = glm::dot(rVec, rVec);
		if (r2 > h * h) continue;
		float r = std::sqrt(r2);
		if (r < 1e-6f) continue;

		glm::vec3 grad = SpikyGradientKernel(spikyCoeff, h, r, rVec);

		glm::vec3 vij = pj->velocity - pi->velocity;
		omega += vij.x * grad.y - vij.y * grad.x;
	}

	outOmegas[particleIdx] = omega;
}

bool PhysicsEngine::FindLocalFluidSurface(const glm::vec3& bMin, const glm::vec3& bMax,
	uint16_t boundaryLayer, uint16_t boundaryMask,
	float& outSurfaceY, float& outRho0) {
	if (bMin.x > fluidBoundsMax.x || bMax.x < fluidBoundsMin.x) return false;

	float surfaceY = -INFINITY;
	bool foundSurface = false;

	float rho0Sum = 0.0f;
	int rho0Count = 0;
	float fallbackRestDensity = 0.0f;
	bool haveFallback = false;

	for (size_t idx = 0; idx < allFluidParticles.size(); idx++) {
		if (!fluidSurfaceQualifies[idx]) continue;
		FluidParticle* c = allFluidParticles[idx];

		if (!layerOverlap(c->collisionLayer, c->collisionMask, boundaryLayer, boundaryMask)) continue;

		glm::vec3 r(c->collisionRadius);
		glm::vec3 pMin = c->position - r;
		glm::vec3 pMax = c->position + r;
		if (pMin.x > bMax.x || pMax.x < bMin.x) continue;

		surfaceY = std::max(surfaceY, c->position.y);
		foundSurface = true;

		if (!haveFallback) { fallbackRestDensity = 0.0f; haveFallback = true; }

		if (pMin.y > bMax.y || pMax.y < bMin.y) continue;
		rho0Sum += c->density;
		rho0Count++;
	}
	if (!foundSurface) return false;

	outSurfaceY = surfaceY;
	outRho0 = (rho0Count > 0) ? (rho0Sum / (float)rho0Count) : fallbackRestDensity;
	return true;
}

void PhysicsEngine::ApplyRigidBuoyancy(float dtSub) {
	const float linearDragCoeff = 8.0f;   
	const float angularDragCoeff = 0.8f; 

	for (auto& b : rigidBoundaries) {
		if (!b.surfaceValid || !b.rb) continue;

		float area = 0.0f;
		glm::vec3 centroid(0.0f);
		if (!ComputeSubmergedRegion(b.worldEdges, b.surfaceY, area, centroid)) continue;

		glm::vec3 F(0.0f, b.rho0 * 9.8f * area, 0.0f);
		glm::vec3 r = centroid - b.worldCenter;
		float torque = r.x * F.y - r.y * F.x;

		b.rb->velocity += b.rb->inverseMass * F * dtSub;
		b.rb->angularVelocity += b.rb->inverseInertia * torque * dtSub;

		float submergedFraction = glm::clamp(area / std::max(b.totalArea, 1e-6f), 0.0f, 1.0f);

		glm::vec3 dragForce = -linearDragCoeff * b.rb->velocity * submergedFraction;
		float dragTorque = -angularDragCoeff * b.rb->angularVelocity * submergedFraction;

		b.rb->velocity += b.rb->inverseMass * dragForce * dtSub;
		b.rb->angularVelocity += b.rb->inverseInertia * dragTorque * dtSub;
	}
}

void PhysicsEngine::ApplySoftBuoyancy(float dtSub) {
	const float linearDragCoeff = 8.0f; 

	for (auto& b : softBoundaries) {
		if (!b.surfaceValid) continue;

		std::vector<Edge> edges;
		edges.reserve(b.worldEdges.size());
		for (auto& se : b.worldEdges) edges.push_back(se.edge);

		float area = 0.0f;
		glm::vec3 centroid(0.0f);
		if (!ComputeSubmergedRegion(edges, b.surfaceY, area, centroid)) continue;

		float totalForceY = b.rho0 * 9.8f * area;

		float submergedFraction = glm::clamp(area / std::max(b.totalArea, 1e-6f), 0.0f, 1.0f);

		float depthSum = 0.0f;
		std::vector<float> depths(b.sb->MassAggregate.size(), 0.0f);
		for (int i = 0; i < (int)b.sb->MassAggregate.size(); i++) {
			float d = b.surfaceY - b.sb->MassAggregate[i]->worldPos.y;
			depths[i] = std::max(0.0f, d);
			depthSum += depths[i];
		}
		if (depthSum < 1e-6f) continue;

		for (int i = 0; i < (int)b.sb->MassAggregate.size(); i++) {
			if (depths[i] <= 0.0f) continue;
			PointMass* pm = b.sb->MassAggregate[i].get();
			float weight = depths[i] / depthSum;
			pm->acceleration += glm::vec3(0.0f, totalForceY * weight * pm->inverseMass, 0.0f);

			glm::vec3 dragForce = -linearDragCoeff * pm->velocity * submergedFraction;
			pm->acceleration += dragForce * pm->inverseMass;
		}
	}
}

void PhysicsEngine::ResolvePBF(float delta) {
	if (allFluidParticles.empty()) return;

	int pbfSubsteps = 2;
	float dtSub = delta / pbfSubsteps;

	GenerateRigidBoundaries();
	GenerateSoftBoundaries();

	if (particleIndices.size() != allFluidParticles.size()) {
		particleIndices.resize(allFluidParticles.size());
		std::iota(particleIndices.begin(), particleIndices.end(), 0);
	}

	for (int sub = 0; sub < pbfSubsteps; sub++) {
		RefreshRigidBoundariesEdges();
		RefreshSoftBoundariesEdges();

		{
			TIME_BLOCK("Predict Positions");
			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) {
					FluidParticle* p = allFluidParticles[i];
					p->velocity += dtSub * glm::vec3(0.0f, -9.8f, 0.0f);
					p->predictedPosition = p->position + dtSub * p->velocity;
				});
		}

		std::vector<glm::vec3> predicted;
		predicted.resize(allFluidParticles.size());
		float max_smoothing = 0.0f;
		for (size_t i = 0; i < allFluidParticles.size(); i++) {
			predicted[i] = allFluidParticles[i]->predictedPosition;
			max_smoothing = std::max(allFluidParticles[i]->smoothingRadius, max_smoothing);
		}

		{
			TIME_BLOCK("Grid Build");
			SpatialGrid.cellSize = max_smoothing;
			SpatialGrid.Build(predicted);
		}

		fluidNeighbors.assign(allFluidParticles.size(), {});
		{
			TIME_BLOCK("Neighbor Query");
			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) {
					SpatialGrid.QueryNeighbourCells(predicted[i], fluidNeighbors[i]);
				});
		}

		{
			TIME_BLOCK("Surface Qualify");
			ComputeFluidSurfaceQualification();
			RefreshRigidBoundariesSurface();
			RefreshSoftBoundariesSurface();
		}

		{
			TIME_BLOCK("Rigid buoyancy");
			ApplyRigidBuoyancy(dtSub);
		}

		{
			TIME_BLOCK("Soft buoyancy");
			ApplySoftBuoyancy(dtSub);
		}

		int solveIterations = 4;
		{
			TIME_BLOCK("Solve PBF");
			for (int iter = 0; iter < solveIterations; iter++)
			{
				{
					TIME_BLOCK("Solve Lambda");
					std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
						[&](int i) { SolvePBFLambda(i, fluidNeighbors[i]); });
				}

				if (correctedPositions.size() != allFluidParticles.size())
					correctedPositions.resize(allFluidParticles.size());

				{
					TIME_BLOCK("Solve Position");
					std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
						[&](int i) { SolvePBFPosition(i, fluidNeighbors[i], correctedPositions); });

					std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
						[&](int i) { allFluidParticles[i]->predictedPosition = correctedPositions[i]; });
				}
			}
		}
		{
			TIME_BLOCK("Fluid-Rigid collision");
			ResolveFluidRigidContacts(dtSub);
		}

		{
			TIME_BLOCK("Fluid-Soft collision");    
			ResolveFluidSoftContacts(dtSub);
		}

		{
			TIME_BLOCK("Velocity update");
			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) {
					FluidParticle* p = allFluidParticles[i];
					p->velocity = (1 / dtSub) * (p->predictedPosition - p->position);
					p->position = p->predictedPosition;
				});
		}

		{
			TIME_BLOCK("Fluid-Rigid collision impulse");
			ResolveFluidRigidImpulses(dtSub);
		}

		{
			TIME_BLOCK("Fluid-Soft collision impulse");
			ResolveFluidSoftImpulses(dtSub);
		}

		{
			TIME_BLOCK("Vorticity");
			if (vorticityOmegas.size() != allFluidParticles.size()) vorticityOmegas.resize(allFluidParticles.size());
			if (vorticityForces.size() != allFluidParticles.size()) vorticityForces.resize(allFluidParticles.size());

			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) { ComputeVorticity(i, fluidNeighbors[i], vorticityOmegas); });

			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) { SolveVorticityConfinement(i, fluidNeighbors[i], vorticityOmegas, vorticityForces); });

			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) { allFluidParticles[i]->velocity += vorticityForces[i] * dtSub; });
		}

		{
			TIME_BLOCK("Viscosity");
			if (viscosityDeltas.size() != allFluidParticles.size())
				viscosityDeltas.resize(allFluidParticles.size());

			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) { SolveXSPHViscosity(i, fluidNeighbors[i], viscosityDeltas); });

			std::for_each(std::execution::par_unseq, particleIndices.begin(), particleIndices.end(),
				[&](int i) { allFluidParticles[i]->velocity += viscosityDeltas[i]; });
		}
	}
}

//Fracture Physics
void PhysicsEngine::ProcessFractures() {
	if (pendingFractures.empty()) return;

	std::unordered_map<Object*, PendingFracture> strongest; 
	for (auto& pf : pendingFractures) {
		auto it = strongest.find(pf.obj);
		if (it == strongest.end() || pf.impulse > it->second.impulse)
			strongest[pf.obj] = pf;
	}
	pendingFractures.clear();

	for (auto& [obj, pf] : strongest) {
		FractureObject(obj, pf.worldPoint);
	}
}

void PhysicsEngine::FractureObject(Object* source, const glm::vec3& worldImpactPoint) {
	RenderComponent* srcRC = source->GetComponent<RenderComponent>();
	TransformComponent* srcTC = source->GetComponent<TransformComponent>();
	FractureComponent* srcFC = source->GetComponent<FractureComponent>();
	if (!srcRC || !srcTC || !srcFC) return;
	EditorManager::getInstance().SetSelectedObject(nullptr);

	std::vector<glm::vec3> localPoly;
	for (auto& e : srcRC->edges) localPoly.push_back(e.start);
	if (localPoly.size() < 3) return;

	glm::vec3 srcSize = srcTC->size;
	std::vector<glm::vec3> scaledPoly;
	scaledPoly.reserve(localPoly.size());
	for (auto& p : localPoly) scaledPoly.push_back(p * srcSize);

	glm::mat4 invWorld = glm::inverse(srcTC->WorldMatrix);
	glm::vec3 localImpact = glm::vec3(invWorld * glm::vec4(worldImpactPoint, 1.0f));
	glm::vec3 scaledImpact = localImpact * srcSize;
	if (!PointInPolygon(scaledImpact, scaledPoly))
		scaledImpact = ClosestPointOnPolygon(scaledImpact, scaledPoly);

	std::vector<glm::vec3> seeds = {};
	{
		TIME_BLOCK("Generate fracture seeds");
		seeds = GenerateFractureSeeds(scaledPoly, scaledImpact, srcFC->shardCount);
	}

	float totalArea = std::abs(ComputeSignedArea(scaledPoly));

	std::vector<Shard> shards;

	for (int i = 0; i < (int)seeds.size(); i++) {
		{
			TIME_BLOCK("Compute voronoi cell");
			std::vector<glm::vec3> cell = ComputeVoronoiCell(scaledPoly, seeds, i);
			if (cell.size() < 3) continue;
			float area = std::abs(ComputeSignedArea(cell));
			if (area < srcFC->minFragmentArea * totalArea) continue;

			glm::vec3 centroid(0.0f);
			for (auto& p : cell) centroid += p;
			centroid /= (float)cell.size();
			shards.push_back({ cell, centroid });
		}
	}

	if (shards.size() < 2) return;

	std::vector<Object*> shardObjects;
	for (int i = 0; i < shards.size(); i++)
	{
		{
			TIME_BLOCK("Create shards");
			Shard s = shards[i];
			shardObjects.push_back(CreateFractureShard(source, s.points, s.centroid, i));
		}
	}

	ObjectManager::getInstance().RemoveObject(source);
}

Object* PhysicsEngine::CreateFractureShard(Object* source, const std::vector<glm::vec3>& scaledShardPoints, const glm::vec3& scaledCentroidLocal, int index) {
	std::unique_ptr<Object> shard;
	shard = std::make_unique<Object>(Shader(source->shader.vertexPath.c_str(), source->shader.fragmentPath.c_str()));
	shard->name = source->name + "_shard_" + std::to_string(index);

	for (auto& c : source->components) {
		std::string compLabel = "Fracture: CopyTo " + c->Name;
		DebugTimer t(compLabel);
		c->CopyTo(shard.get());
	}

	glm::vec3 srcSize = source->GetComponent<TransformComponent>()->size;
	glm::vec3 invSrcSize(
		srcSize.x != 0.0f ? 1.0f / srcSize.x : 1.0f,
		srcSize.y != 0.0f ? 1.0f / srcSize.y : 1.0f,
		srcSize.z != 0.0f ? 1.0f / srcSize.z : 1.0f);

	int n = (int)scaledShardPoints.size();
	std::vector<float> polyVerts;
	polyVerts.reserve(n * 5);
	for (auto& p : scaledShardPoints) {
		glm::vec3 recentered = p - scaledCentroidLocal;
		glm::vec3 unscaledP = p * invSrcSize;
		glm::vec2 uv = source->GetComponent<RenderComponent>()->ComputeUVAtLocalPoint(unscaledP);
		polyVerts.insert(polyVerts.end(), { recentered.x, recentered.y, 0.0f, uv.x, uv.y });
	}

	PolygonShape shardShape;
	shardShape.vertices = polyVerts;
	shard->GetComponent<RenderComponent>()->SetShape(shardShape);

	shard->GetComponent<TransformComponent>()->size = glm::vec3(1.0f);

	glm::vec3 unscaledCentroidLocal = scaledCentroidLocal * invSrcSize;
	glm::vec3 worldCentroid = source->GetComponent<TransformComponent>()->ProjectToWorld(unscaledCentroidLocal);
	shard->GetComponent<TransformComponent>()->SetRotationCenter(shard->GetComponent<RenderComponent>()->GetCenter());
	shard->GetComponent<TransformComponent>()->UpdateWorldPosition(worldCentroid);

	shard->GetComponent<CollisionComponent>()->calculateBoundingCircle();

	RigidBodyComponent* shardRB = shard->GetComponent<RigidBodyComponent>();
	RigidBodyComponent* srcRB = source->GetComponent<RigidBodyComponent>();

	if (!shardRB) {
		shard->AddComponent(std::make_unique<RigidBodyComponent>(shard.get()));
		shardRB = shard->GetComponent<RigidBodyComponent>();
	}

	float sourceArea = source->GetComponent<RenderComponent>()->GetArea();
	float shardArea = shard->GetComponent<RenderComponent>()->GetArea();

	float shardMass;
	if (srcRB) {
		float sourceMass = 1.0f / srcRB->inverseMass;
		shardMass = std::max(sourceMass * (shardArea / sourceArea), 0.001f);
	}
	else {
		shardMass = std::max(shardArea * source->GetComponent<FractureComponent>()->restDensity, 0.001f);
	}

	glm::vec3 r = worldCentroid - source->GetComponent<TransformComponent>()->GetWorldPosition();
	shardRB->inverseMass = 1.0f / shardMass;

	if (srcRB) {
		shardRB->velocity = srcRB->velocity + glm::vec3(-srcRB->angularVelocity * r.y, srcRB->angularVelocity * r.x, 0.0f);
		shardRB->angularVelocity = srcRB->angularVelocity;
	}
	else {
		shardRB->velocity = glm::vec3(0.0f);
		shardRB->angularVelocity = 0.0f;
	}
	shardRB->CalculateInertia();

	FractureComponent* srcFC = source->GetComponent<FractureComponent>();
	if (srcFC->generation < srcFC->maxFractureGenerations) {
		shard->GetComponent<FractureComponent>()->generation = srcFC->generation + 1;
	}
	else {
		shard->RemoveComponent<FractureComponent>();
	}

	Object* shardPtr = shard.get();
	allObjects->push_back(std::move(shard));
	return shardPtr;
}

glm::vec3 PhysicsEngine::ClosestPointOnPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon) {
	float bestDist = INFINITY;
	glm::vec3 best = polygon.empty() ? glm::vec3(0) : polygon[0];
	int n = (int)polygon.size();
	for (int i = 0; i < n; i++) {
		glm::vec3 a = polygon[i];
		glm::vec3 b = polygon[(i + 1) % n];
		glm::vec3 ab = b - a;
		float len = glm::length(ab);
		if (len < 1e-8f) continue;
		float t = glm::clamp(glm::dot(point - a, ab / len) / len, 0.0f, 1.0f);
		glm::vec3 closest = a + (ab / len) * t * len;
		float d = glm::length(point - closest);
		if (d < bestDist) { bestDist = d; best = closest; }
	}
	return best;
}

std::vector<glm::vec3> PhysicsEngine::ClipPolygonHalfPlane(const std::vector<glm::vec3>& poly, const glm::vec3& normal, float offset) {
	std::vector<glm::vec3> out;
	int n = (int)poly.size();
	if (n == 0) return out;

	for (int i = 0; i < n; i++) {
		const glm::vec3& curr = poly[i];
		const glm::vec3& next = poly[(i + 1) % n];

		float dCurr = glm::dot(curr, normal) - offset;
		float dNext = glm::dot(next, normal) - offset;

		bool currInside = dCurr <= 0.0f;
		bool nextInside = dNext <= 0.0f;

		if (currInside) out.push_back(curr);
		if (currInside != nextInside) {
			float t = dCurr / (dCurr - dNext);
			out.push_back(curr + t * (next - curr));
		}
	}
	return out;
}

std::vector<glm::vec3> PhysicsEngine::ComputeVoronoiCell(const std::vector<glm::vec3>& polygon, const std::vector<glm::vec3>& seeds, int seedIndex) {
	std::vector<glm::vec3> cell = polygon;
	const glm::vec3& s = seeds[seedIndex];

	for (int j = 0; j < (int)seeds.size(); j++) {
		if (j == seedIndex) continue;
		const glm::vec3& o = seeds[j];

		glm::vec3 mid = (s + o) * 0.5f;
		glm::vec3 normal = glm::normalize(o - s); 
		float offset = glm::dot(mid, normal);

		cell = ClipPolygonHalfPlane(cell, normal, offset);
		if (cell.empty()) break;
	}
	return cell;
}

std::vector<glm::vec3> PhysicsEngine::GenerateFractureSeeds(const std::vector<glm::vec3>& polygon, const glm::vec3& impactPoint, int count) {
	std::vector<glm::vec3> seeds;
	seeds.push_back(impactPoint); 

	glm::vec3 bmin(INFINITY), bmax(-INFINITY);
	for (auto& p : polygon) { bmin = glm::min(bmin, p); bmax = glm::max(bmax, p); }

	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> ux(bmin.x, bmax.x);
	std::uniform_real_distribution<float> uy(bmin.y, bmax.y);

	float minSeedDist = glm::length(bmax - bmin) * 0.15f;
	int attempts = 0;
	while ((int)seeds.size() < count && attempts < count * 50) {
		attempts++;
		glm::vec3 candidate(ux(rng), uy(rng), 0.0f);
		if (!PointInPolygon(candidate, polygon)) continue;

		bool tooClose = false;
		for (auto& s : seeds) {
			if (glm::length(s - candidate) < minSeedDist) { tooClose = true; break; }
		}
		if (tooClose) continue;

		seeds.push_back(candidate);
	}
	return seeds;
}