#pragma once
#include "Object.h"
#include "RigidBodyComponent.h"
#include "FractureComponent.h"
#include "CollisionComponent.h"
#include "ForceGenerator.h"
#include "BAHNode.h"
#include "DebugPoint.h"
#include "Constraint.h"
#include "XPBDConstraint.h"
#include "ContactConstraint.h"
#include "XPBDContactConstraint.h"
#include "ContactID.h"
#include "PointMass.h"
#include "DebugTimer.h"
#include "ObjectManager.h"
#include <numeric>
#include <algorithm>
#include <random>

// Sutherland Hodgman

struct ClipVertex {
	ContactID id;
	glm::vec3 position;
};

struct ContactPoint {
	ContactID id;
	glm::vec3 normal;
	glm::vec3 point;
	float penetration;
};

struct CollisionData {
	bool isColliding = false;
	float penetration = 0.0f;
	glm::vec3 normal = glm::vec3(0.0f);
	std::vector<Edge> objAEdges;
	std::vector<Edge> objBEdges;
};

// SAT

struct SeparatingAxis {
	glm::vec3 normal;
	glm::vec3 start;
	glm::vec3 end;
};

struct Projection {
	float min;
	float max;

	bool Overlaps(const Projection& other) const {
		return !(this->max < other.min || other.max < this->min);
	}
};

//Fracture
struct PendingFracture {
	Object* obj;
	glm::vec3 worldPoint;
	float impulse;
};

class SoftBodyComponent;
struct SoftEdge;

class PhysicsEngine
{
protected:
	struct ForceRegistration {
		Object* object;
		ForceGenerator* fg;
	};

	typedef std::vector<ForceRegistration> Registry;
	Registry ForceRegistrations;
public:
	PhysicsEngine(const PhysicsEngine&) = delete;
	void operator=(const PhysicsEngine&) = delete;

	static PhysicsEngine& getInstance() {
		static PhysicsEngine instance;
		return instance;
	}

	void Setup(std::vector<std::unique_ptr<Object>>* objects);
	void ProcessPhysics(float delta);
	//Force
	void RegisterForce(Object* object, ForceGenerator* fg);
	void UnRegisterForce(Object* object, ForceGenerator* fg);
	void UnRegisterAllForce(Object* object);
	void ClearRegistry();

	//Collision detection
	std::vector<ContactPoint> allContactPoints;
	BAHNode<BoundingCircle> root;
	BAHNode<BoundingCircle>* RegisterBoundingAreaNode(Object* obj, BoundingCircle boundingCircle);
	void UnRegisterBoundingAreaNode(Object* obj);
	void ResolveContacts(PotentialContact* contacts, unsigned numContacts);
	bool ResolveSoftPointSoftEdgeContacts(PhysicsBody pointBody, PointMass* pointMass,
		SoftBodyComponent* otherSb, const std::vector<SoftEdge>& otherEdges,
		float vertexRadius, const glm::vec3* forcedAxis = nullptr);
	bool ResolveRigidVertexSoftEdgeContacts(const glm::vec3& checkPoint, PhysicsBody rigidBody, SoftBodyComponent* sb, const std::vector<SoftEdge>& edges, float vertexRadius, const glm::vec3* forcedAxis = nullptr);
	bool ResolveCircleCircleContacts(PhysicsBody bodyA, PhysicsBody bodyB, float rA, float rB);
	bool ResolveCirclePolygonContacts(PhysicsBody circle, PhysicsBody polygon, float radius, std::vector<Edge> edges, const glm::vec3* forcedAxis = nullptr);
	bool ResolvePolygonPolygonContacts(PhysicsBody bodyA, PhysicsBody bodyB);
	CollisionData SAT(Object* objA, Object* objB);
	std::vector<ContactPoint> GenerateContactPoints(CollisionData collisionData);

	glm::vec3 ComputeSoftSoftAxis(SoftBodyComponent* sbA, const std::vector<SoftEdge>& edgesA,
		SoftBodyComponent* sbB, const std::vector<SoftEdge>& edgesB, bool* outValid);
	glm::vec3 ComputeRigidSoftAxis(PhysicsBody rigidBody, const std::vector<Edge>& rigidEdgesLocal, SoftBodyComponent* sb, bool* outValid);
	Projection ProjectOntoAxis(std::vector<glm::vec3>& vertices, SeparatingAxis axis);
	float ComputeSignedArea(const std::vector<glm::vec3>& vertices);
	Edge FindMostParallelEdge(const std::vector<Edge>& edges, const glm::vec3& normal);
	Edge FindMostAntiParallelEdge(const std::vector<Edge>& edges, const glm::vec3& normal);
	int ClipSegmentToLine(ClipVertex vOut[2], const ClipVertex vIn[2], int numInPoints,
		const glm::vec3& normal, float offset, int referenceEdgeIndex, bool isA_Reference, int clipPlaneId);

	//PGS Constraint resolution
	std::vector<ContactCache> contactsCache;
	std::vector<Constraint*> registeredPGSConstraints;
	void UpdateContactCache();
	void UnRegisterTemporaryConstraint();
	void RegisterPGSConstraint(Constraint* constraint);
	void UnRegisterPGSConstraint(Constraint* constraint);
	void ResolvePGSConstraintsForSubstep(float dtSub);
	void ResolvePGSConstraints(float delta);

	//XPBD Constraint resolution 
	std::vector<PointMass*> allSoftBodyPointMasses;
	std::vector<PointMass*> allSoftBodyProxies;
	std::vector<XPBDConstraint*> registeredXPBDConstraints;
	void RegisterXPBDConstraint(XPBDConstraint* constraint);
	void UnRegisterXPBDConstraint(XPBDConstraint* constraint);
	void UnRegisterTemporaryXPBDConstraint();
	void ResolveXPBDConstraint(float delta);

	//Fracture physics
	std::vector<PendingFracture> pendingFractures;

	void ProcessFractures();
	void FractureObject(Object* source, const glm::vec3& worldImpactPoint);
	Object* CreateFractureShard(Object* source, const std::vector<glm::vec3>& localShardPoints, const glm::vec3& shardCentroidLocal, int index);
	bool PointInPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon);
	glm::vec3 ClosestPointOnPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon);
	std::vector<glm::vec3> ClipPolygonHalfPlane(const std::vector<glm::vec3>& poly, const glm::vec3& normal, float offset);
	std::vector<glm::vec3> ComputeVoronoiCell(const std::vector<glm::vec3>& polygon, const std::vector<glm::vec3>& seeds, int seedIndex);
	std::vector<glm::vec3> GenerateFractureSeeds(const std::vector<glm::vec3>& polygon, const glm::vec3& impactPoint, int count);
	
	// Others
	PhysicsBody GetBodyFromObject(Object* obj);
private:
	PhysicsEngine() = default;
	std::vector<std::unique_ptr<Object>>* allObjects;
};

