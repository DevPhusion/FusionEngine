#pragma once
#include "Object.h"
#include "RigidBodyComponent.h"
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
#include <numeric>

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
	void ResolvePGSConstraints(float delta);

	//XPBD Constraint resolution 
	std::vector<PointMass*> allSoftBodyPointMasses;
	std::vector<XPBDConstraint*> registeredXPBDConstraints;
	void RegisterXPBDConstraint(XPBDConstraint* constraint);
	void UnRegisterXPBDConstraint(XPBDConstraint* constraint);
	void UnRegisterTemporaryXPBDConstraint();
	void ResolveXPBDConstraint(float delta);
private:
	PhysicsEngine() = default;
	std::vector<std::unique_ptr<Object>>* allObjects;
};

