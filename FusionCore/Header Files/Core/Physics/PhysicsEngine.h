#pragma once
#include "../../Objects/Object.h"
#include "../../Components/RigidBodyComponent.h"
#include "../../Components/FractureComponent.h"
#include "../../Components/CollisionComponent.h"
#include "Forces/ForceGenerator.h"
#include "Collision/BAHNode.h"
#include "Collision/DebugPoint.h"
#include "Constraint/PGSConstraint/Constraint.h"
#include "Constraint/XPBDConstraint/XPBDConstraint.h"
#include "Constraint/PGSConstraint/ContactConstraint.h"
#include "Constraint/XPBDConstraint/XPBDContactConstraint.h"
#include "Collision/ContactID.h"
#include "../../Objects/PointMass.h"
#include "../DebugTimer.h"
#include "../ObjectManager.h"
#include "Collision/SpatialHashGrid.h"
#include "../../Components/FluidComponent.h"
#include "../Editor/Windows/Console.h"
#include <numeric>
#include <algorithm>
#include <random>
#include <numbers>
#include <execution>

// Collision registration
struct ObjectPairHash {
	size_t operator()(const std::pair<Object*, Object*>& p) const {
		return std::hash<Object*>()(p.first) ^ (std::hash<Object*>()(p.second) << 1);
	}
};

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

// Rigid body
struct RigidContactRecord {
	TransformComponent* tcA = nullptr;
	TransformComponent* tcB = nullptr;
	bool staticA = false;
	bool staticB = false;
	glm::vec3 normal = glm::vec3(0.0f);
	float penetration = 0.0f;
};

//Soft body
class SoftBodyComponent;
struct SoftEdge;

struct SoftRigidContact {
	bool hit = false;
	glm::vec3 normal = glm::vec3(0);
	float penetration = 0.0f;
	glm::vec3 point = glm::vec3(0);
	int rigidIndex = -1;
	float accumNormalImpulse = 0.0f;  
	float accumTangentImpulse = 0.0f; 
};

struct RigidSoftContact {
	bool hit = false;
	glm::vec3 normal = glm::vec3(0);
	float penetration = 0.0f;
	glm::vec3 point = glm::vec3(0);
	int softIndex = -1;
	int edgeIdx = -1;
	float edgeT = 0.0f;
	float accumNormalImpulse = 0.0f;
	float accumTangentImpulse = 0.0f;
};

struct RigidVertex {
	int rigidIndex = -1;
	glm::vec3 worldPos = glm::vec3(0);
};

//Fracture
struct PendingFracture {
	Object* obj;
	glm::vec3 worldPoint;
	float impulse;
};

struct Shard { 
	std::vector<glm::vec3> points; 
	glm::vec3 centroid; 
};

//Fluid
struct ClosestPointOnEdge {
	bool found = false;
	float dist = INFINITY;
	glm::vec3 point = glm::vec3(0.0f);
	glm::vec3 normal = glm::vec3(0.0f);
	int edgeIdx = -1;
	float edgeT = 0.0f;
};

struct RigidBoundary {
	Object* obj;
	RigidBodyComponent* rb;
	TransformComponent* tc;
	std::vector<Edge> localEdges;
	std::vector<Edge> worldEdges;
	glm::vec3 worldCenter;
	float totalArea;
	float surfaceY = 0.0f;
	bool surfaceValid = false;
	float rho0 = 0.0f;
	uint16_t collisionLayer = 0xFFFF;
	uint16_t collisionMask = 0xFFFF;
};

struct FluidRigidContact {
	bool hit = false;
	int rigidIndex = -1;
	glm::vec3 normal = glm::vec3(0.0f);
	glm::vec3 point = glm::vec3(0.0f);
	float penetration = 0.0f;
};

struct SoftBoundary {
	Object* obj = nullptr;
	SoftBodyComponent* sb = nullptr;
	std::vector<SoftEdge> worldEdges;
	glm::vec3 worldCenter = glm::vec3(0.0f);
	bool valid = true;
	float totalArea = 0.0f;
	float surfaceY = 0.0f;
	bool surfaceValid = false;
	float rho0 = 0.0f;
	uint16_t collisionLayer = 0xFFFF;
	uint16_t collisionMask = 0xFFFF;
};

struct FluidSoftContact {
	bool hit = false;
	glm::vec3 normal = glm::vec3(0.0f);
	float penetration = 0.0f;
	glm::vec3 point = glm::vec3(0.0f);
	int softIndex = -1; 
	int edgeIdx = -1;   
	float edgeT = 0.0f; 
};

// Ray casting

struct RayCastHit {
	bool hit = false;
	Object* object = nullptr;
	glm::vec3 point = glm::vec3(0.0f);   
	glm::vec3 normal = glm::vec3(0.0f);  
	float distance = INFINITY;          
	int edgeIndex = -1;                  
	bool isSoftBody = false;
};

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

	//Collision detection and resolution
	std::vector<ContactPoint> allContactPoints;
	BAHNode<BoundingCircle> root;
	std::vector<RigidBoundary> rigidBoundaries;
	std::vector<SoftBoundary> softBoundaries;      
	std::vector<SoftRigidContact> softRigidContacts;
    std::vector<RigidVertex> rigidVertices; 
	std::vector<RigidSoftContact> rigidSoftContacts;
	std::vector<std::vector<glm::vec3>> rigidSoftAxis;
	std::vector<std::vector<bool>> rigidSoftAxisValid;
	std::vector<FluidRigidContact> fluidRigidContacts;
	std::vector<FluidSoftContact> fluidSoftContacts;

	//Broad phase
	BAHNode<BoundingCircle>* RegisterBoundingAreaNode(Object* obj, BoundingCircle boundingCircle);
	void UnRegisterBoundingAreaNode(Object* obj);

	void ResolveContacts(PotentialContact* contacts, unsigned numContacts);
	
	//Fluid collision
	FluidSoftContact DetectFluidSoftContact(const glm::vec3& particlePos, float radius, const SoftBoundary& soft);
	template<typename BoundaryT, typename ContactT, typename DetectFn>
	void ResolveFluidBoundaryContactsGeneric(std::vector<FluidParticle*>& particles, std::vector<int>& indices,
		std::vector<BoundaryT>& boundaries, std::vector<ContactT>& outContacts, DetectFn detect);
	void ResolveFluidSoftContacts(float dtSub);
	void ResolveFluidSoftImpulses(float dtSub);
	FluidRigidContact DetectFluidRigidContact(const glm::vec3& particlePos, float radius, const RigidBoundary& rigid);
	void ResolveFluidRigidContacts(float dtSub);
	void ResolveFluidRigidImpulses(float dtSub);
	
	//Soft body collision
	bool ResolveSoftPointSoftEdgeContacts(PhysicsBody pointBody, PointMass* pointMass,
		SoftBodyComponent* otherSb, const std::vector<SoftEdge>& otherEdges,
		float vertexRadius, const glm::vec3* forcedAxis = nullptr);
	SoftRigidContact DetectSoftRigidContact(const glm::vec3& pmPos, float radius, const RigidBoundary& rigid, const glm::vec3* forcedAxis);
	void ResolveSoftRigidContacts(float dtSub);
	void ResolveSoftRigidImpulses(float dtSub);
	void ApplySoftRigidPositionCorrection();
	RigidSoftContact DetectRigidSoftContact(const glm::vec3& vertexPos, float radius, const SoftBoundary& soft, const glm::vec3* forcedAxis);
	void ResolveRigidSoftContacts(float dtSub);
	void ResolveRigidSoftImpulses(float dtSub);
	void ApplyRigidSoftPositionCorrection();
	
	//Rigid body collision
	std::vector<RigidContactRecord> rigidContactRecords;
	bool ResolveCircleCircleContacts(PhysicsBody bodyA, PhysicsBody bodyB, float rA, float rB);
	bool ResolveCirclePolygonContacts(PhysicsBody circle, PhysicsBody polygon, float radius, std::vector<Edge> edges, const glm::vec3* forcedAxis = nullptr);
	bool ResolvePolygonPolygonContacts(PhysicsBody bodyA, PhysicsBody bodyB);
	void ApplyRigidPositionCorrection();

	//Static body collision
	bool ResolveStaticCirclePolygon(TransformComponent* circleTc, float radius, bool circleStatic,
		TransformComponent * polyTc, const std::vector<Edge>& polyLocalEdges, bool polyStatic);
	bool ResolveStaticPolygonPolygon(Object * objA, TransformComponent * tcA, bool staticA,
		Object * objB, TransformComponent * tcB, bool staticB);
	void ApplyStaticPositionCorrection(TransformComponent * tcA, bool staticA,
		TransformComponent * tcB, bool staticB, const glm::vec3 & normal, float penetration);

	//Notify collision
	std::unordered_map<std::pair<Object*, Object*>, CollisionEventData, ObjectPairHash> currentFrameCollisions;
	std::unordered_map<std::pair<Object*, Object*>, CollisionEventData, ObjectPairHash> previousFrameCollisions;

	CollisionType ClassifyCollisionType(Object* objA, Object* objB);
	void BroadcastCollision(Object* objA, Object* objB, CollisionType type,
		const glm::vec3& point, const glm::vec3& normal, float penetration);
	void BroadcastFluidRigidContacts();
	void BroadcastFluidSoftContacts();

	void RecordCollisionPair(Object* objA, Object* objB, CollisionType type,
		const glm::vec3& point, const glm::vec3& normal, float penetration);
	void ResolveCollisionEnterExit();
	void PurgeObjectFromCollisionTracking(Object* obj);

	//Helper functions
	CollisionData SAT(Object* objA, Object* objB);
	std::vector<ContactPoint> GenerateContactPoints(CollisionData collisionData);
	void GenerateRigidVertices();
	void GenerateRigidBoundaries();
	void RefreshRigidBoundariesEdges();
	void GenerateSoftBoundaries();
	void RefreshSoftBoundariesEdges();
	bool PointInPolygon(const glm::vec3& p, const std::vector<glm::vec3>& starts, const std::vector<glm::vec3>& ends);
	bool PointInPolygon(const glm::vec3& p, const std::vector<Edge>& edges);
	bool PointInPolygon(const glm::vec3& p, const std::vector<SoftEdge>& edges);
	bool PointInPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon);
	ClosestPointOnEdge GetClosestPointOnEdge(const glm::vec3& p, const std::vector<glm::vec3>& starts,
		const std::vector<glm::vec3>& ends, const glm::vec3& interiorRefPoint);
	glm::vec3 ComputeSoftSoftAxis(SoftBodyComponent* sbA, const std::vector<SoftEdge>& edgesA,
		SoftBodyComponent* sbB, const std::vector<SoftEdge>& edgesB, bool* outValid);
	glm::vec3 ComputeRigidSoftAxis(const RigidBoundary& rigid, const SoftBoundary& soft, bool* outValid);
	void RefreshRigidSoftAxes();
	Projection ProjectOntoAxis(std::vector<glm::vec3>& vertices, SeparatingAxis axis);
	float ComputeSignedArea(const std::vector<glm::vec3>& vertices);
	template<typename ProjectFnA, typename ProjectFnB>
	glm::vec3 FindMinOverlapAxis(const std::vector<glm::vec3>& axes, ProjectFnA projectA, ProjectFnB projectB, bool& outValid);
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
	void ResolvePGSConstraintsForSubstep(float dtSub, float frameDelta);
	void ResolvePGSConstraints(float delta);

	//XPBD Constraint resolution 
	std::vector<PointMass*> allSoftBodyPointMasses;
	std::vector<PointMass*> allSoftBodyProxies;
	std::vector<XPBDConstraint*> registeredXPBDConstraints;
	void RegisterXPBDConstraint(XPBDConstraint* constraint);
	void UnRegisterXPBDConstraint(XPBDConstraint* constraint);
	void UnRegisterTemporaryXPBDConstraint();
	void ResolveXPBDConstraints(float delta);

	//PBF resolution
	SpatialHashGrid SpatialGrid = SpatialHashGrid(1.0f);
	std::vector<FluidParticle*> allFluidParticles;
	std::vector<int> particleIndices;
	std::vector<std::vector<int>> fluidNeighbors;
	std::vector<glm::vec3> correctedPositions;
	std::vector<glm::vec3> viscosityDeltas;
	std::vector<float> vorticityOmegas;
	std::vector<glm::vec3> vorticityForces;
	std::vector<char> fluidSurfaceQualifies;
	glm::vec3 fluidBoundsMin = glm::vec3(INFINITY);
	glm::vec3 fluidBoundsMax = glm::vec3(-INFINITY);
	float buoyancyMinNeighbours = 4;
	float Poly6Coefficient(float h);
	float SpikyCoefficient(float h);
	float Poly6Kernel(float poly6Coeff, float h2, float r2);
	glm::vec3 SpikyGradientKernel(float spikyCoeff, float h, float r, glm::vec3 rVec);
	void ComputeVorticity(int particleIdx, std::vector<int>& neighboursIdx, std::vector<float>& outOmegas);
	void SolvePBFLambda(int particleIdx, std::vector<int>& neighboursIdx);
	void SolvePBFPosition(int particleIdx, std::vector<int>& neighboursIdx, std::vector<glm::vec3>& outPositions);
	void SolveXSPHViscosity(int particleIdx, std::vector<int>& neighboursIdx, std::vector<glm::vec3>& outDeltas);
	void SolveVorticityConfinement(int particleIdx, std::vector<int>& neighboursIdx, std::vector<float>& omega, std::vector<glm::vec3>& outForce);
	bool FindLocalFluidSurface(const glm::vec3& bMin, const glm::vec3& bMax, 
		uint16_t boundaryLayer, uint16_t boundaryMask,
		float& outSurfaceY, float& outRho0);
	void ComputeFluidSurfaceQualification();
	void RefreshRigidBoundariesSurface();
	void RefreshSoftBoundariesSurface();
	bool ComputeSubmergedRegion(const std::vector<Edge>& worldEdges,
		float surfaceY, float& outArea, glm::vec3& outCentroid);
	void ApplyRigidBuoyancy(float dtSub);
	void ApplySoftBuoyancy(float dtSub);

	void ResolvePBF(float delta);

	//Fracture physics
	std::vector<PendingFracture> pendingFractures;

	void ProcessFractures();
	void FractureObject(Object* source, const glm::vec3& worldImpactPoint);
	Object* CreateFractureShard(Object* source, const std::vector<glm::vec3>& localShardPoints, const glm::vec3& shardCentroidLocal, int index);
	glm::vec3 ClosestPointOnPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon);
	std::vector<glm::vec3> ClipPolygonHalfPlane(const std::vector<glm::vec3>& poly, const glm::vec3& normal, float offset);
	std::vector<glm::vec3> ComputeVoronoiCell(const std::vector<glm::vec3>& polygon, const std::vector<glm::vec3>& seeds, int seedIndex);
	std::vector<glm::vec3> GenerateFractureSeeds(const std::vector<glm::vec3>& polygon, const glm::vec3& impactPoint, int count);

	//Ray casting
	RayCastHit RayCast(const glm::vec3& origin, const glm::vec3& direction, float length,
		std::optional<uint16_t> collisionLayer = std::nullopt,
		const std::vector<Object*>& ignoreObjects = {});

	std::vector<RayCastHit> RayCastAll(const glm::vec3& origin, const glm::vec3& direction, float length,
		std::optional<uint16_t> collisionLayer = std::nullopt,
		const std::vector<Object*>& ignoreObjects = {});

	bool RaySegmentIntersect(const glm::vec3& origin, const glm::vec3& dir, float length,
		const glm::vec3& segStart, const glm::vec3& segEnd,
		float& outT, glm::vec3& outPoint, glm::vec3& outNormal);

	bool RayCircleIntersect(const glm::vec3& origin, const glm::vec3& dir, float length,
		const glm::vec3& center, float radius,
		float& outT, glm::vec3& outPoint, glm::vec3& outNormal);

	void RayCastObject(const glm::vec3& origin, const glm::vec3& dir, float length,
		Object* obj, std::vector<RayCastHit>& outHits);

	// Others
	PhysicsBody GetBodyFromObject(Object* obj);
private:
	PhysicsEngine() = default;
	void PhysicsModeChangeEvent();
	std::vector<std::unique_ptr<Object>>* allObjects;
};

