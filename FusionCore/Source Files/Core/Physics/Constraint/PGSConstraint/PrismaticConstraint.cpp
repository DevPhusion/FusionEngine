#include "../../../../../Header Files/Core/Physics/Constraint/PGSConstraint/PrismaticConstraint.h"
#include "../../../../../Header Files/Core/Rendering/Renderer.h"

PrismaticConstraint::PrismaticConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, glm::vec3 dir) :
    Constraint(objectA, objectB, attachPointA, attachPointB) {
    this->dir = dir;
    this->Name = "Prismatic Constraint";
}

void PrismaticConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
    if (objectA.obj == nullptr || objectB.obj == nullptr) {
        return;
    }

    glm::vec3 globalPointA = (objectA.pm != nullptr)
        ? *objectA.position
        : glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));

    glm::vec3 globalPointB = (objectB.pm != nullptr)
        ? *objectB.position
        : glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

    glm::vec3 rA = globalPointA - *objectA.position;
    glm::vec3 rB = globalPointB - *objectB.position;

    JacobianRow jacobianLinear, jacobianTheta;
    SolverRow rowLinear, rowTheta;

    glm::vec3 t = glm::vec3(-dir.y, dir.x, 0.0f);

    jacobianLinear.linearA = t;
    jacobianLinear.linearB = -t;
    jacobianLinear.angularA = (rA.x * t.y - rA.y * t.x);
    jacobianLinear.angularB = -(rB.x * t.y - rB.y * t.x);

    jacobianTheta.linearA = glm::vec3(0);
    jacobianTheta.linearB = glm::vec3(0);
    jacobianTheta.angularA = 1;
    jacobianTheta.angularB = -1;

    float klinear = 0.0f;
    float ktheta = 0.0f;
    if (objectA.invMass != nullptr && objectA.invInertia != nullptr) {
        klinear += *objectA.invMass * glm::length2(jacobianLinear.linearA) + *objectA.invInertia * (jacobianLinear.angularA * jacobianLinear.angularA);
        ktheta += *objectA.invInertia;
    }
    if (objectB.invMass != nullptr && objectB.invInertia != nullptr) {
        klinear += *objectB.invMass * glm::length2(jacobianLinear.linearB) + *objectB.invInertia * (jacobianLinear.angularB * jacobianLinear.angularB);
        ktheta += *objectB.invInertia;
    }

    float biasLinear = (beta * glm::dot(dir, t)) / delta;
    float biasTheta = *objectB.rotation - *objectA.rotation;

    rowLinear.jacobian = jacobianLinear;
    rowLinear.effectiveMass = (klinear > 0.0f) ? 1.0f / klinear : 0.0f;
    rowLinear.bias = biasLinear;
    rowLinear.maxLambda = INFINITY;
    rowLinear.minLambda = -INFINITY;
    rowLinear.objectA = objectA;
    rowLinear.objectB = objectB;
    rowLinear.parentConstraint = this;
    rowLinear.lambda = cacheLambda[0];
    rowLinear.softnessCFM = 0.0f;

    rowTheta.jacobian = jacobianTheta;
    rowTheta.effectiveMass = (ktheta > 0.0f) ? 1.0f / ktheta : 0.0f;
    rowTheta.bias = biasTheta;
    rowTheta.maxLambda = INFINITY;
    rowTheta.minLambda = -INFINITY;
    rowTheta.objectA = objectA;
    rowTheta.objectB = objectB;
    rowTheta.parentConstraint = this;
    rowTheta.lambda = cacheLambda[1];
    rowTheta.softnessCFM = 0.0f;

    linearRowOffset = static_cast<int>(rows.size());
    rows.push_back(rowLinear);
    thetaRowOffset = static_cast<int>(rows.size());
    rows.push_back(rowTheta);
}

void PrismaticConstraint::PostSolve(std::vector<SolverRow>& allRows) {
    cacheLambda[0] = allRows[linearRowOffset].lambda;
    cacheLambda[1] = allRows[thetaRowOffset].lambda;
}

void PrismaticConstraint::SetObjectA(PhysicsBody obj) {
    Constraint::SetObjectA(obj);

    if (objectA.obj != nullptr && objectB.obj != nullptr) {
        glm::vec3 pA = objectA.obj->GetComponent<TransformComponent>()->GetWorldPosition();
        glm::vec3 pB = objectB.obj->GetComponent<TransformComponent>()->GetWorldPosition();
        this->dir = pB - pA;
    }
}

void PrismaticConstraint::SetObjectB(PhysicsBody obj) {
    Constraint::SetObjectB(obj);

    if (objectA.obj != nullptr && objectB.obj != nullptr) {
        glm::vec3 pA = objectA.obj->GetComponent<TransformComponent>()->GetWorldPosition();
        glm::vec3 pB = objectB.obj->GetComponent<TransformComponent>()->GetWorldPosition();
        this->dir = pB - pA;
    }
}


void PrismaticConstraint::ProcessInspectorUI(Object* parent) {
    Constraint::ProcessInspectorUI(parent);

    if (objectA.obj && objectB.obj) {
        ImGui::Text("Locked direction ");
        ImGui::BeginDisabled();
        float d[2] = { dir.x, dir.y };
        ImGui::InputFloat2("##Locked direction", d);
        ImGui::EndDisabled();
        if (ImGui::Button("Re-lock direction")) {
            EditorManager::getInstance().BeginEdit({ parent }, true);
            glm::vec3 pA = objectA.obj->GetComponent<TransformComponent>()->GetWorldPosition();
            glm::vec3 pB = objectB.obj->GetComponent<TransformComponent>()->GetWorldPosition();
            this->dir = pB - pA;
            EngineManager::getInstance().SceneChangeEvent();
            EditorManager::getInstance().EndEdit({ parent });
        }
    }
}

void PrismaticConstraint::DrawConstraintGizmo() {
    if (objectA.obj == nullptr || objectB.obj == nullptr) return;

    glm::vec3 top = GetAttachWorldA();
    glm::vec3 bot = GetAttachWorldB();

    glm::vec3 segment = bot - top;
    float totalLength = glm::length(segment);
    if (totalLength < 0.0001f) return;

    glm::vec3 segDir = segment / totalLength;

    const float dashLength = 0.3f;
    const float gapLength = 0.1f;
    const float thickness = 6.0f;
    const glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    float travelled = 0.0f;

    while (travelled + dashLength <= totalLength) {
        glm::vec3 dashStart = top + segDir * travelled;
        glm::vec3 dashEnd = top + segDir * (travelled + dashLength);
        Renderer::getInstance().DrawLine(dashStart, dashEnd, color, thickness);
        travelled += dashLength + gapLength;
    }

    if (travelled < totalLength) {
        glm::vec3 dashStart = top + segDir * travelled;
        Renderer::getInstance().DrawLine(dashStart, bot, color, thickness);
    }
}

std::shared_ptr<Constraint> PrismaticConstraint::Clone() {
    std::shared_ptr<PrismaticConstraint> constraint = std::make_shared<PrismaticConstraint>(PhysicsBody(), PhysicsBody(), attachPointA, attachPointB, dir);
    constraint->CopyBaseFieldsFrom(this);
    return constraint;
}

void PrismaticConstraint::Serialize(BinaryWriter& w) {
    Constraint::Serialize(w);
    w.Write(dir);
}

void PrismaticConstraint::Deserialize(BinaryReader& r) {
    Constraint::Deserialize(r);
    dir = r.Read<glm::vec3>();
}