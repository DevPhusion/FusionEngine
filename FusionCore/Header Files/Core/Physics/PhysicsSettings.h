#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include "../Files/BinaryWriter.h"
#include "../Files/BinaryReader.h"

struct PhysicsSettings {
	// World
	glm::vec3 gravity = glm::vec3(0.0f, -9.8f, 0.0f);

	// Rigid body
	int   pgsFrameIterations = 30;     // ResolvePGSConstraints
	int   pgsSubstepIterations = 8;      // ResolvePGSConstraintsForSubstep
	float contactBeta = 0.2f;   // ContactConstraint
	float contactStaticFriction = 0.4f;
	float contactDynamicFriction = 0.6f;
	float rigidPositionCorrection = 0.2f;
	float rigidSlop = 0.005f;
	float rigidSleepLinear = 0.001f;
	float rigidSleepAngular = 0.001f;
	float dragStiffness = 150.0f; // mouse drag spring (per kg)
	float dragDamping = 24.7f;

	// Soft body (XPBD)
	int   xpbdSubsteps = 8;
	int   xpbdPositionIterations = 4;
	float softAreaCompliance = 0.0f;   // applied on soft body rebuild
	float softTriAreaCompliance = 0.001f; // applied on soft body rebuild
	float softGasMaxForcePerEdge = 50.0f;
	float softSoftCompliance = 0.0001f; // XPBDContactConstraint 
	float softSoftStaticFriction = 0.6f;
	float softSoftDynamicFriction = 0.4f;

	// Soft <-> Rigid contacts
	int   srContactIterations = 4;
	float srBeta = 0.2f;
	float srSlop = 0.0005f;
	float srMaxBiasVelocity = 2.0f;
	float srStaticFriction = 0.4f;
	float srDynamicFriction = 0.6f;
	float srPositionCorrection = 0.2f;
	float softRigidRestitution = 0.2f;   // soft point vs rigid
	float rigidSoftRestitution = 0.0f;   // rigid vertex vs soft
	float rigidVertexRadius = 0.01f;

	// Fluid / Gas (PBF)
	int   pbfSubsteps = 2;
	int   pbfSolveIterations = 4;
	float pbfSCorrK = 0.1f;   // tensile instability strength
	float pbfSCorrDeltaQ = 0.2f;   // fraction of smoothing radius
	float gasMinTemperature = 1.0f;

	// Fluid boundary contacts (rigid + soft)
	int   fluidContactIterations = 4;
	float fluidContactBeta = 0.2f;
	float fluidContactSlop = 0.0005f;
	float fluidContactRestitution = 0.0f;
	float fluidMaxBiasVelocity = 2.0f;
	float fluidPositionCorrection = 0.2f;
	float fluidPenetrationClamp = 3.0f;   // x particle radius

	// Buoyancy
	float buoyancyLinearDrag = 8.0f;
	float buoyancyAngularDrag = 0.8f;
	int   buoyancyMinNeighbours = 4;

	// Fracture
	float fractureSeedMinDistance = 0.15f;  // fraction of bbox diagonal
	float fractureMinShardMass = 0.001f;

	void Serialize(BinaryWriter& w) const {
		// World
		w.Write(gravity);

		// Rigid body
		w.Write(pgsFrameIterations);
		w.Write(pgsSubstepIterations);
		w.Write(contactBeta);
		w.Write(contactStaticFriction);
		w.Write(contactDynamicFriction);
		w.Write(rigidPositionCorrection);
		w.Write(rigidSlop);
		w.Write(rigidSleepLinear);
		w.Write(rigidSleepAngular);
		w.Write(dragStiffness);
		w.Write(dragDamping);

		// Soft body (XPBD)
		w.Write(xpbdSubsteps);
		w.Write(xpbdPositionIterations);
		w.Write(softAreaCompliance);
		w.Write(softTriAreaCompliance);
		w.Write(softGasMaxForcePerEdge);
		w.Write(softSoftCompliance);
		w.Write(softSoftStaticFriction);
		w.Write(softSoftDynamicFriction);

		// Soft <-> Rigid contacts
		w.Write(srContactIterations);
		w.Write(srBeta);
		w.Write(srSlop);
		w.Write(srMaxBiasVelocity);
		w.Write(srStaticFriction);
		w.Write(srDynamicFriction);
		w.Write(srPositionCorrection);
		w.Write(softRigidRestitution);
		w.Write(rigidSoftRestitution);
		w.Write(rigidVertexRadius);

		// Fluid / Gas (PBF)
		w.Write(pbfSubsteps);
		w.Write(pbfSolveIterations);
		w.Write(pbfSCorrK);
		w.Write(pbfSCorrDeltaQ);
		w.Write(gasMinTemperature);

		// Fluid boundary contacts
		w.Write(fluidContactIterations);
		w.Write(fluidContactBeta);
		w.Write(fluidContactSlop);
		w.Write(fluidContactRestitution);
		w.Write(fluidMaxBiasVelocity);
		w.Write(fluidPositionCorrection);
		w.Write(fluidPenetrationClamp);

		// Buoyancy
		w.Write(buoyancyLinearDrag);
		w.Write(buoyancyAngularDrag);
		w.Write(buoyancyMinNeighbours);

		// Fracture
		w.Write(fractureSeedMinDistance);
		w.Write(fractureMinShardMass);
	}

	void Deserialize(BinaryReader& r) {
		*this = PhysicsSettings();   

		// World
		gravity = r.Read<glm::vec3>();

		// Rigid body
		pgsFrameIterations = r.Read<int>();
		pgsSubstepIterations = r.Read<int>();
		contactBeta = r.Read<float>();
		contactStaticFriction = r.Read<float>();
		contactDynamicFriction = r.Read<float>();
		rigidPositionCorrection = r.Read<float>();
		rigidSlop = r.Read<float>();
		rigidSleepLinear = r.Read<float>();
		rigidSleepAngular = r.Read<float>();
		dragStiffness = r.Read<float>();
		dragDamping = r.Read<float>();

		// Soft body (XPBD)
		xpbdSubsteps = r.Read<int>();
		xpbdPositionIterations = r.Read<int>();
		softAreaCompliance = r.Read<float>();
		softTriAreaCompliance = r.Read<float>();
		softGasMaxForcePerEdge = r.Read<float>();
		softSoftCompliance = r.Read<float>();
		softSoftStaticFriction = r.Read<float>();
		softSoftDynamicFriction = r.Read<float>();

		// Soft <-> Rigid contacts
		srContactIterations = r.Read<int>();
		srBeta = r.Read<float>();
		srSlop = r.Read<float>();
		srMaxBiasVelocity = r.Read<float>();
		srStaticFriction = r.Read<float>();
		srDynamicFriction = r.Read<float>();
		srPositionCorrection = r.Read<float>();
		softRigidRestitution = r.Read<float>();
		rigidSoftRestitution = r.Read<float>();
		rigidVertexRadius = r.Read<float>();

		// Fluid / Gas (PBF)
		pbfSubsteps = r.Read<int>();
		pbfSolveIterations = r.Read<int>();
		pbfSCorrK = r.Read<float>();
		pbfSCorrDeltaQ = r.Read<float>();
		gasMinTemperature = r.Read<float>();

		// Fluid boundary contacts
		fluidContactIterations = r.Read<int>();
		fluidContactBeta = r.Read<float>();
		fluidContactSlop = r.Read<float>();
		fluidContactRestitution = r.Read<float>();
		fluidMaxBiasVelocity = r.Read<float>();
		fluidPositionCorrection = r.Read<float>();
		fluidPenetrationClamp = r.Read<float>();

		// Buoyancy
		buoyancyLinearDrag = r.Read<float>();
		buoyancyAngularDrag = r.Read<float>();
		buoyancyMinNeighbours = r.Read<int>();

		// Fracture
		fractureSeedMinDistance = r.Read<float>();
		fractureMinShardMass = r.Read<float>();

		// Guard against zero/negative loop counts from a bad file
		pgsFrameIterations = std::max(pgsFrameIterations, 1);
		pgsSubstepIterations = std::max(pgsSubstepIterations, 1);
		xpbdSubsteps = std::max(xpbdSubsteps, 1);
		xpbdPositionIterations = std::max(xpbdPositionIterations, 1);
		srContactIterations = std::max(srContactIterations, 1);
		pbfSubsteps = std::max(pbfSubsteps, 1);
		pbfSolveIterations = std::max(pbfSolveIterations, 1);
		fluidContactIterations = std::max(fluidContactIterations, 1);
	}
};