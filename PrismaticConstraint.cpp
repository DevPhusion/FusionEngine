#include "PrismaticConstraint.h"

PrismaticConstraint::PrismaticConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, glm::vec3 dir, float weightA, float weightB) :
    Constraint(objectA, objectB, attachPointA, attachPointB, weightA, weightB) {
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

    jacobianLinear.linearA = t * weightA;
    jacobianLinear.linearB = -t * weightB;
    jacobianLinear.angularA = weightA * (rA.x * t.y - rA.y * t.x);
    jacobianLinear.angularB = -weightB * (rB.x * t.y - rB.y * t.x);

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
            glm::vec3 pA = objectA.obj->GetComponent<TransformComponent>()->GetWorldPosition();
            glm::vec3 pB = objectB.obj->GetComponent<TransformComponent>()->GetWorldPosition();
            this->dir = pB - pA;
        }
    }
}

void PrismaticConstraint::WarmStartSoftBody() {
    if (objectA.obj == nullptr || objectB.obj == nullptr) return;
    if (objectA.pm == nullptr && objectB.pm == nullptr) return;

    glm::vec3 globalPointA = (objectA.pm != nullptr)
        ? *objectA.position
        : glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));

    glm::vec3 globalPointB = (objectB.pm != nullptr)
        ? *objectB.position
        : glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

    glm::vec3 t = glm::vec3(-dir.y, dir.x, 0.0f);

    glm::vec3 positionError = globalPointB - globalPointA;
    float perpError = glm::dot(positionError, t);
    glm::vec3 correction = t * perpError;

    if (objectA.pm != nullptr) {
        *objectA.position += correction * weightA;
    }
    if (objectB.pm != nullptr) {
        *objectB.position -= correction * weightB;
    }
}

void PrismaticConstraint::ProcessConstraintDisplay() {
    RenderComponent* rc = constraintDisplay->GetComponent<RenderComponent>();
    TransformComponent* tc = constraintDisplay->GetComponent<TransformComponent>();

    if (objectA.obj == nullptr || objectB.obj == nullptr || !canDrawConstraint) {
        rc->SetEnabled(false);
        return;
    }

    glm::vec3 top = objectA.obj->GetComponent<TransformComponent>()->GetTransformedPoint(attachPointA);
    glm::vec3 bot = objectB.obj->GetComponent<TransformComponent>()->GetTransformedPoint(attachPointB);

    glm::vec2 topVert = tc->GetTransformedPoint(top, true);
    glm::vec2 botVert = tc->GetTransformedPoint(bot, true);

    glm::vec2 segment = botVert - topVert;
    float totalLength = glm::length(segment);
    if (totalLength < 0.0001f) return;

    glm::vec2 segDir = segment / totalLength;
    float nx = -segDir.y;
    float ny = segDir.x;

    const float thickness = 0.01f;
    const float dashLength = 0.05f;
    const float gapLength = 0.03f;
    const float halfT = thickness * 0.5f;
    const float offsetX = nx * halfT;
    const float offsetY = ny * halfT;

    std::vector<float>        vertices;
    std::vector<unsigned int> indices;

    float travelled = 0.0f;

    while (travelled + dashLength <= totalLength) {
        glm::vec2 dashStart = topVert + segDir * travelled;
        glm::vec2 dashEnd = topVert + segDir * (travelled + dashLength);

        unsigned int base = static_cast<unsigned int>(vertices.size() / 5);

        vertices.insert(vertices.end(), { dashStart.x + offsetX, dashStart.y + offsetY, 0.0f, 0.0f, 0.0f });
        vertices.insert(vertices.end(), { dashEnd.x + offsetX, dashEnd.y + offsetY, 0.0f, 1.0f, 0.0f });
        vertices.insert(vertices.end(), { dashStart.x - offsetX, dashStart.y - offsetY, 0.0f, 0.0f, 1.0f });
        vertices.insert(vertices.end(), { dashEnd.x - offsetX, dashEnd.y - offsetY, 0.0f, 1.0f, 1.0f });

        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 1);

        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 3);

        travelled += dashLength + gapLength;
    }

    if (travelled < totalLength) {
        glm::vec2 dashStart = topVert + segDir * travelled;
        glm::vec2 dashEnd = botVert;

        unsigned int base = static_cast<unsigned int>(vertices.size() / 5);

        vertices.insert(vertices.end(), { dashStart.x + offsetX, dashStart.y + offsetY, 0.0f, 0.0f, 0.0f });
        vertices.insert(vertices.end(), { dashEnd.x + offsetX, dashEnd.y + offsetY, 0.0f, 1.0f, 0.0f });
        vertices.insert(vertices.end(), { dashStart.x - offsetX, dashStart.y - offsetY, 0.0f, 0.0f, 1.0f });
        vertices.insert(vertices.end(), { dashEnd.x - offsetX, dashEnd.y - offsetY, 0.0f, 1.0f, 1.0f });

        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 1);

        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    rc->UpdateShape(vertices, indices);
    rc->SetEnabled(true);
}