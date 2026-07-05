#include "PhysicsEngine.h"
#include "SoftBodyComponent.h"

void PhysicsEngine::Setup(std::vector<std::unique_ptr<Object>>* objects) {
	this->allObjects = objects;
}

void PhysicsEngine::ProcessPhysics(float delta) {
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Pause) {
		return;
	}

	UnRegisterTemporaryXPBDConstraint();
	UnRegisterTemporaryConstraint();

	for (int i = 0; i < ForceRegistrations.size(); i++)
	{
		ForceRegistrations[i].fg->updateForce(ForceRegistrations[i].object, delta);
	}

	for (int i = 0; i < allObjects->size(); i++)
	{
		if ((*allObjects)[i]->HasComponent<RigidBodyComponent>()) {
			(*allObjects)[i]->GetComponent<RigidBodyComponent>()->IntegrateVelocities(delta);
		}
		if ((*allObjects)[i]->HasComponent<SoftBodyComponent>()) {
			(*allObjects)[i]->GetComponent<SoftBodyComponent>()->ProcessSoftBody(delta);
		}
		if ((*allObjects)[i]->HasComponent<CollisionComponent>()) {
			(*allObjects)[i]->GetComponent<RenderComponent>()->color = glm::vec4(1);
		}
	}

	PotentialContact potentialContacts[200];
	unsigned totalContacts = root.getPotentialContacts(potentialContacts, 200);
	if (totalContacts > 0) {
		ResolveContacts(potentialContacts, totalContacts);
	}

	ResolvePGSConstraints(delta);
	ResolveXPBDConstraint(delta);

	UpdateContactCache();

	for (int i = 0; i < allObjects->size(); i++)
	{
		if ((*allObjects)[i]->HasComponent<RigidBodyComponent>()) {
			(*allObjects)[i]->GetComponent<RigidBodyComponent>()->IntegratePositions(delta);
		}
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
			std::cout << "Remove" << std::endl;

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

		RenderComponent* rcA = objA->GetComponent<RenderComponent>();
		RenderComponent* rcB = objB->GetComponent<RenderComponent>();
		TransformComponent* tcA = objA->GetComponent<TransformComponent>();
		TransformComponent* tcB = objB->GetComponent<TransformComponent>();

		float rA = -1;
		float rB = -1;

		std::visit([&](auto&& s) {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, CircleShape>) {
				rA = s.radius;
			}
		}, rcA->currentShape);

		std::visit([&](auto&& s) {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, CircleShape>) {
				rB = s.radius;
			}
		}, rcB->currentShape);

		PhysicsBody bodyA = PhysicsBody();
		RigidBodyComponent* pcA = objA->GetComponent<RigidBodyComponent>();
		SoftBodyComponent* sbA = objA->GetComponent<SoftBodyComponent>();
		bodyA.obj = objA;

		if (tcA) {
			bodyA.position = &tcA->worldPosition;
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
			if (rA > 0.0f && rB > 0.0f && tcA->size.x == tcA->size.y && tcB->size.x == tcB->size.y) {
				collisionResult = ResolveCircleCircleContacts(bodyA, bodyB, rA * tcA->size.x, rB * tcB->size.x);
			}
			else if ((rA > 0.0f && tcA->size.x == tcA->size.y) || (rB > 0.0f && tcB->size.x == tcB->size.y)) {
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

				collisionResult = ResolveCirclePolygonContacts(circle, poly, radius, poly.obj->GetComponent<RenderComponent>()->edges);
			}
			else {
				collisionResult = ResolvePolygonPolygonContacts(bodyA, bodyB);
			}
		}
		// Rigidbody & Softbody
		if ((sbA || sbB) && !(sbA && sbB)) {
			PhysicsBody rigidBody = (sbA) ? bodyB : bodyA;
			SoftBodyComponent* sb = (sbA) ? sbA : sbB;

			bool anyPointContact = false;
			for (int i = 0; i < sb->MassAggregate.size(); i++)
			{
				PhysicsBody pointMassBody = sb->MassAggregate[i]->body;
				bool hit = ResolveCirclePolygonContacts(pointMassBody, rigidBody, sb->MassAggregate[i]->pointRadius, rigidBody.obj->GetComponent<RenderComponent>()->edges);
				anyPointContact = anyPointContact || hit;
			}

			std::vector<SoftEdge> softBodyEdges = sb->GetEdgesFromMassAggregate();
			const float rigidVertexRadius = 0.01f;

			std::vector<Edge> rigidEdges = rigidBody.obj->GetComponent<RenderComponent>()->edges;

			for (int i = 0; i < rigidEdges.size(); i++)
			{
				glm::vec3 point = rigidBody.obj->GetComponent<TransformComponent>()->ProjectToWorld(rigidEdges[i].start);

				bool hit = ResolveRigidVertexSoftEdgeContacts(point, rigidBody, sb, softBodyEdges, rigidVertexRadius);
				anyPointContact = anyPointContact || hit;
			}

			collisionResult = anyPointContact;
		}
		//Soft body & Soft body
		if (sbA && sbB) {
			std::vector<SoftEdge> edgesA = sbA->GetEdgesFromMassAggregate();
			std::vector<SoftEdge> edgesB = sbB->GetEdgesFromMassAggregate();

			bool anyPointContact = false;

			for (auto& pm : sbA->MassAggregate) {
				if (ResolveSoftPointSoftEdgeContacts(pm->body, pm.get(), sbB, edgesB, pm->pointRadius))
					anyPointContact = true;
			}

			for (auto& pm : sbB->MassAggregate) {
				if (ResolveSoftPointSoftEdgeContacts(pm->body, pm.get(), sbA, edgesA, pm->pointRadius))
					anyPointContact = true;
			}

		}

		if (EngineManager::getInstance().debugMode && collisionResult) {
			objA->GetComponent<RenderComponent>()->color = glm::vec4(0, 1, 0, 1);
			objB->GetComponent<RenderComponent>()->color = glm::vec4(0, 1, 0, 1);
		}
		else if (EngineManager::getInstance().debugMode) {
			objA->GetComponent<RenderComponent>()->color = glm::vec4(1, 0, 0, 1);
			objB->GetComponent<RenderComponent>()->color = glm::vec4(1, 0, 0, 1);
		}
	}
}

bool PhysicsEngine::ResolveSoftPointSoftEdgeContacts(PhysicsBody pointBody, PointMass* pointMass,
	SoftBodyComponent* otherSb, const std::vector<SoftEdge>& otherEdges,
	float vertexRadius) {
	glm::vec3 center = pointMass->worldPos;

	bool centerInside = false;
	for (int e = 0; e < (int)otherEdges.size(); e++) {
		glm::vec3 p1 = otherEdges[e].edge.start;
		glm::vec3 p2 = otherEdges[e].edge.end;

		if (((p1.y > center.y) != (p2.y > center.y)) &&
			(center.x < (p2.x - p1.x) * (center.y - p1.y) / (p2.y - p1.y) + p1.x)) {
			centerInside = !centerInside;
		}
	}

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
		contactNormal = bestNormal;
		penetration = vertexRadius + bestDist;
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
	constraint->isTemporary = true;
	RegisterXPBDConstraint(constraint);

	return true;
}

bool PhysicsEngine::ResolveRigidVertexSoftEdgeContacts(const glm::vec3& checkPoint, PhysicsBody rigidBody,
	SoftBodyComponent* sb, const std::vector<SoftEdge>& edges, float vertexRadius) {
	glm::vec3 center = checkPoint;
	
	bool centerInside = false;
	for (int e = 0; e < (int)edges.size(); e++) {
		glm::vec3 p1 = edges[e].edge.start;
		glm::vec3 p2 = edges[e].edge.end;

		if (((p1.y > center.y) != (p2.y > center.y)) &&
			(center.x < (p2.x - p1.x) * (center.y - p1.y) / (p2.y - p1.y) + p1.x)) {
			centerInside = !centerInside;
		}
	}

	float bestDist = INFINITY;
	int bestEdge = -1;
	float bestT = 0.0f;
	glm::vec3 bestPoint = glm::vec3(0.0f);
	glm::vec3 bestNormal = glm::vec3(0.0f);

	for (int e = 0; e < (int)edges.size(); e++) {
		const Edge& edge = edges[e].edge;
		glm::vec3 ab = edge.end - edge.start;
		float len = glm::length(ab);
		if (len < 1e-8f) continue;

		glm::vec3 ac = center - edge.start;
		glm::vec3 abNorm = ab / len;
		float t = glm::clamp(glm::dot(ac, abNorm), 0.0f, len);
		glm::vec3 closest = edge.start + abNorm * t;

		glm::vec3 edgeNormal = glm::normalize(glm::vec3(ab.y, -ab.x, 0.0f));

		glm::vec3 toSoftCenter = sb->CenterPM->worldPos - edge.start;
		
		if (glm::dot(edgeNormal, toSoftCenter) > 0.0f) {
			edgeNormal = -edgeNormal;
		}

		float dist = glm::length(center - closest);
		if (dist < bestDist) {
			bestDist = dist;
			bestPoint = closest;
			bestNormal = edgeNormal;
			bestEdge = e;
			bestT = t / len;
		}
	}

	bool isColliding = false;
	glm::vec3 contactNormal;
	float penetration = 0.0f;

	if (centerInside) {
		contactNormal = bestNormal;
		penetration = vertexRadius + bestDist;
		isColliding = true;
	}
	else if (bestDist < vertexRadius) {
		glm::vec3 dir = center - bestPoint;
		if (glm::length(dir) > 1e-6f && glm::dot(glm::normalize(dir), bestNormal) > 0.5f) {
			contactNormal = glm::normalize(dir);
		}
		else {
			contactNormal = bestNormal;
		}

		penetration = vertexRadius - bestDist;
		isColliding = true;
	}

	if (!isColliding) return false;

	int edgeCount = (int)sb->MassAggregate.size() - 1;
	int i0 = edges[bestEdge].idxA;
	int i1 = edges[bestEdge].idxB;
	PointMass* pm0 = sb->MassAggregate[i0].get();
	PointMass* pm1 = sb->MassAggregate[i1].get();

	float w1 = glm::clamp(bestT, 0.0f, 1.0f);
	float w0 = 1.0f - w1;

	ContactPoint cp;
	cp.point = bestPoint;
	cp.normal = contactNormal;
	cp.penetration = penetration;
	cp.id = ContactID();
	allContactPoints.push_back(cp);

	float cachedLambda0 = 0.0f, cachedLambda1 = 0.0f;
	for (const auto& cached : contactsCache) {
		bool match0 = (cached.objectA == rigidBody.obj && cached.pmB == pm0->body.pm) ||
			(cached.objectB == rigidBody.obj && cached.pmA == pm0->body.pm);
		if (match0) cachedLambda0 = cached.lambda;

		bool match1 = (cached.objectA == rigidBody.obj && cached.pmB == pm1->body.pm) ||
			(cached.objectB == rigidBody.obj && cached.pmA == pm1->body.pm);
		if (match1) cachedLambda1 = cached.lambda;
	}

	if (w0 > 1e-4f) {
		ContactConstraint* c0 = new ContactConstraint(
			rigidBody, pm0->body,
			bestPoint, bestPoint,
			ContactID(), contactNormal, penetration,
			0.2f, 0.4f, 0.6f,
			1.0f, w0);
		c0->SetInitialImpulse(cachedLambda0);
		RegisterPGSConstraint(c0);
	}

	if (w1 > 1e-4f) {
		ContactConstraint* c1 = new ContactConstraint(
			rigidBody, pm1->body,
			bestPoint, bestPoint,
			ContactID(), contactNormal, penetration,
			0.2f, 0.4f, 0.6f,
			1.0f, w1);
		c1->SetInitialImpulse(cachedLambda1);
		RegisterPGSConstraint(c1);
	}

	return true;
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

bool PhysicsEngine::ResolveCirclePolygonContacts(PhysicsBody circle, PhysicsBody polygon, float radius, std::vector<Edge> edges) {
	glm::vec3 center = (circle.position != nullptr) ? *circle.position : glm::vec3(0);

	std::vector<Edge> worldEdges;
	for (const auto& e : edges) {
		Edge we;
		we.start = glm::vec3(*polygon.transformMatrix * glm::vec4(e.start, 1.0f));
		we.end = glm::vec3(*polygon.transformMatrix * glm::vec4(e.end, 1.0f));
		worldEdges.push_back(we);
	}

	float    bestDist = INFINITY;
	glm::vec3 bestPoint = glm::vec3(0.0f);
	glm::vec3 bestNormal = glm::vec3(0.0f);
	bool     centerInside = true;
	glm::vec3 polyCenter = (polygon.position != nullptr) ? *polygon.position : glm::vec3(0);

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

	bool isColliding = false;
	glm::vec3 contactNormal;
	float     penetration;
	glm::vec3 contactPoint = bestPoint;

	if (centerInside) {
		contactNormal = bestNormal;
		penetration = radius + bestDist;
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
	RenderComponent* rcA = objA->GetComponent<RenderComponent>();
	TransformComponent* tcA = objA->GetComponent<TransformComponent>();
	RenderComponent* rcB = objB->GetComponent<RenderComponent>();
	TransformComponent* tcB = objB->GetComponent<TransformComponent>();

	std::vector<Edge> edgesA = rcA->edges;
	std::vector<Edge> edgesB = rcB->edges;

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
	node->removeLeaf();
}

// PGS Constraints

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
			entry.pmA = contact->objectA.pm;   // NEW
			entry.pmB = contact->objectB.pm;   // NEW
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

	const int velocityIterations = 30;
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

	// Bridge to XPBD
	for (auto* constraint : registeredPGSConstraints)
	{
		if (!constraint->isTemporary) continue;
		auto* contact = static_cast<ContactConstraint*>(constraint);

		glm::vec3 normal = contact->normal;
		float lambda = contact->cacheLambda;

		if (!contact->objectA.obj) {
			glm::vec3 deltaX = (*contact->objectA.invMass) * normal * lambda * delta;
			*contact->objectA.position += deltaX;
		}
		else if (!contact->objectB.obj) {
			glm::vec3 deltaX = -(*contact->objectB.invMass) * normal * lambda * delta;
			*contact->objectB.position += deltaX;
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

void PhysicsEngine::ResolveXPBDConstraint(float delta) {
	int substeps = 8;
	float dtSub = delta / substeps;

	for (int i = 0; i < substeps; i++) {
		for (auto& pm : allSoftBodyPointMasses) {
			if (pm->sb->isDragging) pm->ProcessDragForce();

			pm->prevPos = pm->worldPos; 
			pm->velocity += (pm->baseAcceleration + pm->acceleration) * dtSub;
			pm->worldPos += pm->velocity * dtSub;
			pm->acceleration = glm::vec3(0);
		}

		for (auto* constraint : registeredXPBDConstraints) constraint->ResetLambda();

		const int posIterations = 4;
		for (int it = 0; it < posIterations; it++) {
			for (auto* c : registeredXPBDConstraints) {
				c->SolvePosition(dtSub);
			}
		}

		for (auto& pm : allSoftBodyPointMasses) {
			pm->velocity = (pm->worldPos - pm->prevPos) / dtSub;
		}
	}
}