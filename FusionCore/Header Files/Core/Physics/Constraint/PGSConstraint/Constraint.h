#pragma once
#include "../../../../Objects/Object.h"
#include "../../../../Components/TransformComponent.h"
#include "../../../../Components/RigidBodyComponent.h"
#include "../../PhysicsBody.h"
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
    Constraint() = default;
    Constraint(PhysicsBody objectA, PhysicsBody objectB,
        glm::vec3 attachPointA, glm::vec3 attachPointB);
    virtual ~Constraint();

    uint64_t objectIdA = 0;
    uint64_t objectIdB = 0;

    PhysicsBody objectA;
    PhysicsBody objectB;
    glm::vec3 attachPointA = glm::vec3(0.0f);
    glm::vec3 attachPointB = glm::vec3(0.0f);

    std::string Name;
    bool  isTemporary = false;
    float cacheLambda = 0.0f;
    float beta = 0.2f; 

    bool canDrawConstraint = true;

    void SetInitialImpulse(float lambda) { cacheLambda = lambda; }

    void Unregister();

    virtual void Prepare(std::vector<SolverRow>& rows, float delta) = 0;

    virtual void PostIterationClamp(std::vector<SolverRow>& allRows,
        int myRowIndex, int velocityIteration)
    {
        allRows[myRowIndex].lambda = glm::clamp(
            allRows[myRowIndex].lambda,
            allRows[myRowIndex].minLambda,
            allRows[myRowIndex].maxLambda);
    }

    virtual void Serialize(BinaryWriter& w);
    virtual void Deserialize(BinaryReader& r);
    virtual void SetObjectA(PhysicsBody obj);
    virtual void SetObjectB(PhysicsBody obj);

    virtual void PostSolve(std::vector<SolverRow>& allRows) {}

    virtual void DrawConstraintGizmo();

    virtual void ProcessMirroredUI();
    virtual void ProcessInspectorUI(Object* parent);

    glm::vec3 GetAttachWorldA() const;
    glm::vec3 GetAttachWorldB() const;

    bool IsAttachAEditing() const { return attachAEditing; }
    bool IsAttachBEditing() const { return attachBEditing; }
    bool useCenterA = true;
    bool useCenterB = true;
    bool UseCenterA() const { return useCenterA; }
    bool UseCenterB() const { return useCenterB; }

    void OnAttachAMoved(glm::vec3 worldPos);
    void OnAttachBMoved(glm::vec3 worldPos);

protected:
    PointMass* virtualPMA = nullptr;
    PointMass* virtualPMB = nullptr;

    void RemoveMirrorFromObjectB();

    void DrawConstraintLine(const glm::vec4& color, float thickness = 2.0f) const;

    void CopyBaseFieldsFrom(const Constraint* src);

private:
    bool attachAEditing = false;
    bool attachBEditing = false;

    int onDeleteCallbackIdA = -1;
    int onDeleteCallbackIdB = -1;
};