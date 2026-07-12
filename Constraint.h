#pragma once
#include "Object.h"
#include "TransformComponent.h"
#include "RigidBodyComponent.h"
#include "PhysicsBody.h"
#include <string>
#include <vector>

struct JacobianRow {
    glm::vec3 linearA;
    float     angularA;
    glm::vec3 linearB;
    float     angularB;
};

struct SolverRow {
    JacobianRow jacobian;

    float effectiveMass = 0.0f;
    float bias = 0.0f;
    float lambda = 0.0f;
    float softnessCFM = 0.0f;

    float minLambda = -INFINITY;
    float maxLambda = INFINITY;

    PhysicsBody objectA;
    PhysicsBody objectB;

    bool warmStart = true;
    class Constraint* parentConstraint = nullptr;
};

class PointMass;

class Constraint
{
public:
    Constraint(PhysicsBody objectA, PhysicsBody objectB,
        glm::vec3 attachPointA, glm::vec3 attachPointB);
    Constraint() = default;

    uint64_t objectIdA = 0;
    uint64_t objectIdB = 0;

    PhysicsBody objectA;
    PhysicsBody objectB;
    glm::vec3 attachPointA = glm::vec3(0.0f);
    glm::vec3 attachPointB = glm::vec3(0.0f);

    Object* constraintDisplay = nullptr;

    std::string Name;
    bool  isTemporary = false;
    float cacheLambda = 0.0f;
    float beta = 0.2f; // Baumgarte bias tuning

    bool canDrawConstraint = true;

    void SetInitialImpulse(float lambda) { cacheLambda = lambda; }

    void Unregister();

    Object* CreateConstraintDisplay();

    virtual void Prepare(std::vector<SolverRow>& rows, float delta) = 0;

    virtual void PostIterationClamp(std::vector<SolverRow>& allRows,
        int myRowIndex, int velocityIteration)
    {
        allRows[myRowIndex].lambda = glm::clamp(
            allRows[myRowIndex].lambda,
            allRows[myRowIndex].minLambda,
            allRows[myRowIndex].maxLambda);
    }

    virtual std::shared_ptr<Constraint> Clone() = 0;
    virtual void SetObjectA(PhysicsBody obj);
    virtual void SetObjectB(PhysicsBody obj);

    virtual void PostSolve(std::vector<SolverRow>& allRows) {}
    virtual void ProcessConstraintDisplay();
    virtual void ProcessMirroredUI();
    virtual void ProcessInspectorUI(Object* parent);

protected:
    PointMass* virtualPMA = nullptr;
    PointMass* virtualPMB = nullptr;
    Object* attachDisplayA = nullptr;
    Object* attachDisplayB = nullptr;

    void OnPhysicsModeChanged();

    void DestroyDisplayA();
    void DestroyDisplayB();
    void RemoveMirrorFromObjectB();

    void EnsureDisplayA();
    void EnsureDisplayB();

    void OnObjectATransformChanged();
    void OnObjectBTransformChanged();
    void OnDisplayAMoved();
    void OnDisplayBMoved();

    void CopyBaseFieldsFrom(const Constraint* src);

private:
    bool useCenterA = true;
    bool useCenterB = true;
    bool posSetA = false;
    bool posSetB = false;

    int onPhysicsModeChangedCallbackID = -1;
    int onDeleteCallbackIdA = -1;
    int onDeleteCallbackIdB = -1;
    int onTransformCallbackIdA = -1;
    int onTransformCallbackIdB = -1;
};