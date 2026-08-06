#include "../../../../../Header Files/Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "../../../../../Header Files/Core/ObjectManager.h"
#include "../../../../../Header Files/Components/RenderComponent.h"
#include "../../../../../Header Files/Components/TransformComponent.h"
#include "../../../../../Header Files/Components/ConstraintComponent.h"
#include "../../../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../../../Header Files/Core/Editor/ConstraintEditGizmos.h"
#include "../../../../../Header Files/Core/Rendering/Renderer.h"
#include <algorithm>

Constraint::Constraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB)
    : Constraint()
{
    SetObjectA(objectA);
    SetObjectB(objectB);

    this->attachPointA = attachPointA;
    this->attachPointB = attachPointB;

	Renderer::getInstance().constraintEditGizmos->RegisterConstraint(this);
}

Constraint::~Constraint() {
    Renderer::getInstance().constraintEditGizmos->UnregisterConstraint(this);
}

void Constraint::RemoveMirrorFromObjectB()
{
    if (objectB.obj == nullptr) return;
    ConstraintComponent* cc = objectB.obj->GetComponent<ConstraintComponent>();
    if (cc == nullptr) return;

    auto& mv = cc->mirroredConstraints;
    mv.erase(std::remove(mv.begin(), mv.end(), this), mv.end());
}

void Constraint::SetObjectA(PhysicsBody obj)
{
    if (objectA.obj != nullptr)
    {
        objectA.obj->RemoveOnDeleteCallback(onDeleteCallbackIdA);
        if (virtualPMA) {
            objectA.obj->GetComponent<SoftBodyComponent>()->RemoveVirtualProxy(virtualPMA);
            virtualPMA = nullptr;
        }
    }

    objectA = obj;
    objectIdA = obj.obj ? obj.obj->id : 0;
    if (obj.obj == nullptr) return;

    onDeleteCallbackIdA = obj.obj->AddOnDeleteCallback([this]() { SetObjectA(PhysicsBody()); });

    attachPointA = obj.obj->GetComponent<RenderComponent>()->GetCenter();
    useCenterA = true;
    attachAEditing = false;
}

void Constraint::SetObjectB(PhysicsBody obj)
{
    if (objectB.obj != nullptr)
    {
        RemoveMirrorFromObjectB();

        objectB.obj->RemoveOnDeleteCallback(onDeleteCallbackIdB);
        if (virtualPMB) {
            objectB.obj->GetComponent<SoftBodyComponent>()->RemoveVirtualProxy(virtualPMB);
            virtualPMB = nullptr;
        }
    }

    objectB = obj;
    objectIdB = obj.obj ? obj.obj->id : 0;
    if (obj.obj == nullptr) return;

    ConstraintComponent* cc = obj.obj->GetComponent<ConstraintComponent>();
    if (cc == nullptr) {
        auto newCC = std::make_unique<ConstraintComponent>(obj.obj);
        newCC->mirroredConstraints.push_back(this);
        obj.obj->AddComponent(std::move(newCC));
    }
    else {
        cc->mirroredConstraints.push_back(this);
    }

    onDeleteCallbackIdB = obj.obj->AddOnDeleteCallback([this]() { SetObjectB(PhysicsBody()); });

    attachPointB = obj.obj->GetComponent<RenderComponent>()->GetCenter();
    useCenterB = true;
    attachBEditing = false;
}

void Constraint::Unregister()
{
    RemoveMirrorFromObjectB();
    Renderer::getInstance().constraintEditGizmos->UnregisterConstraint(this);
    SetObjectA(PhysicsBody());
    SetObjectB(PhysicsBody());
}

glm::vec3 Constraint::GetAttachWorldA() const
{
    if (objectA.obj == nullptr) return glm::vec3(0.0f);
    TransformComponent* tc = objectA.obj->GetComponent<TransformComponent>();
    if (tc)
        return tc->ProjectToWorld(attachPointA);
    return glm::vec3(0);
}

glm::vec3 Constraint::GetAttachWorldB() const
{
    if (objectB.obj == nullptr) return glm::vec3(0.0f);
    TransformComponent* tc = objectB.obj->GetComponent<TransformComponent>();
    if (tc)
        return tc->ProjectToWorld(attachPointB);
    return glm::vec3(0);
}

void Constraint::OnAttachAMoved(glm::vec3 worldPos)
{
    if (objectA.obj == nullptr) return;
    attachPointA = objectA.obj->GetComponent<TransformComponent>()->ProjectToWorld(worldPos, true);

    SoftBodyComponent* sb = objectA.obj->GetComponent<SoftBodyComponent>();
    if (sb) {
        if (!virtualPMA) {
            virtualPMA = sb->AddVirtualProxy(attachPointA);
        }

        virtualPMA->localPos = attachPointA;
        objectA.pm = virtualPMA;
        objectA.position = &virtualPMA->worldPos;
        objectA.rotation = &virtualPMA->rotation;
        objectA.velocity = &virtualPMA->velocity;
        objectA.angularVelocity = &virtualPMA->angularVelocity;
        objectA.invInertia = &virtualPMA->InverseInertia;
        objectA.invMass = &virtualPMA->inverseMass;
        sb->UpdateVirtualProxy(virtualPMA);
    }
}

void Constraint::OnAttachBMoved(glm::vec3 worldPos)
{
    if (objectB.obj == nullptr) return;
    attachPointB = objectB.obj->GetComponent<TransformComponent>()->ProjectToWorld(worldPos, true);

    SoftBodyComponent* sb = objectB.obj->GetComponent<SoftBodyComponent>();
    if (sb) {
        if (!virtualPMB) {
            virtualPMB = sb->AddVirtualProxy(attachPointB);
        }

        virtualPMB->localPos = attachPointB;
        objectB.pm = virtualPMB;
        objectB.position = &virtualPMB->worldPos;
        objectB.rotation = &virtualPMB->rotation;
        objectB.velocity = &virtualPMB->velocity;
        objectB.angularVelocity = &virtualPMB->angularVelocity;
        objectB.invInertia = &virtualPMB->InverseInertia;
        objectB.invMass = &virtualPMB->inverseMass;
        sb->UpdateVirtualProxy(virtualPMB);
    }
}

void Constraint::DrawConstraintLine(const glm::vec4& color, float thickness) const
{
    if (objectA.obj == nullptr || objectB.obj == nullptr) return;
    Renderer::getInstance().DrawLine(GetAttachWorldA(), GetAttachWorldB(), color, thickness);
}

void Constraint::DrawConstraintGizmo()
{
    if (objectA.obj == nullptr || objectB.obj == nullptr) return;
    DrawConstraintLine(glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
}

void Constraint::ProcessMirroredUI()
{
    ImGui::Text("Linked to %s", objectA.obj ? objectA.obj->name.c_str() : "(none)");
    if (objectA.obj && ImGui::Button("Go to owner"))
        EditorManager::getInstance().SetSelectedObject(objectA.obj);
}

void Constraint::ProcessInspectorUI(Object* parent)
{
    auto& om = ObjectManager::getInstance();

    if (objectA.obj == nullptr && parent != nullptr) {
        PhysicsBody body = PhysicsBody();
        TransformComponent* tc = parent->GetComponent<TransformComponent>();
        RigidBodyComponent* pc = parent->GetComponent<RigidBodyComponent>();
        body.obj = parent;

        if (tc) {
            body.position = &tc->worldPosition;
            body.transformMatrix = &tc->WorldMatrix;
            body.rotation = &tc->rotation;
        }
        if (pc) {
            body.velocity = &pc->velocity;
            body.angularVelocity = &pc->angularVelocity;
            body.invInertia = &pc->inverseInertia;
            body.invMass = &pc->inverseMass;
        }
        SetObjectA(body);
    }

    auto AttachPointWidget = [&](
        const char* popupId,
        Object* currentObj,
        bool& useCenter,
        bool& editing,
        glm::vec3& attachPoint)
        {
            if (currentObj == nullptr) return;

            if (ImGui::Checkbox((std::string("Use Object Center##") + popupId).c_str(), &useCenter))
            {
                if (useCenter)
                {
                    attachPoint = currentObj->GetComponent<RenderComponent>()->GetCenter();
                    if (editing) {
                        editing = false;
                        EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
                    }
                }
            }

            if (!useCenter)
            {
                if (!editing)
                {
                    if (ImGui::Button((std::string("Change Attach Point##") + popupId).c_str())) {
                        editing = true;
                        EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::ConstraintEdit);
                    }
                }
                else
                {
                    if (ImGui::Button((std::string("Confirm##") + popupId).c_str())) {
                        editing = false;
                        EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
                    }
                }
            }
        };

    ImGui::Text("Object A (Owner)");
    {
        char nameBuf[128] = "None";
        if (objectA.obj)
        {
#if defined(_MSC_VER)
            strcpy_s(nameBuf, objectA.obj->name.c_str());
#else
            strncpy(nameBuf, objectA.obj->name.c_str(), sizeof(nameBuf) - 1);
#endif
        }
        ImGui::BeginDisabled();
        ImGui::InputText("##objA_locked", nameBuf, IM_ARRAYSIZE(nameBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
    }
    AttachPointWidget("A", objectA.obj, useCenterA, attachAEditing, attachPointA);

    ImGui::Spacing();

    ImGui::Text("Object B");
    {
        char nameBuf[128] = "None (Click to choose...)";
        if (objectB.obj)
        {
#if defined(_MSC_VER)
            strcpy_s(nameBuf, objectB.obj->name.c_str());
#else
            strncpy(nameBuf, objectB.obj->name.c_str(), sizeof(nameBuf) - 1);
#endif
        }

        ImGui::InputText("##sel_objB", nameBuf, IM_ARRAYSIZE(nameBuf), ImGuiInputTextFlags_ReadOnly);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked())
            ImGui::OpenPopup("ConstraintPickerB");

        if (ImGui::BeginPopup("ConstraintPickerB"))
        {
            ImGui::TextDisabled("Select Object B");
            ImGui::Separator();

            Object* pendingSelection = nullptr;
            for (auto& objPtr : om.allObjects)
            {
                Object* candidate = objPtr.get();
                if (candidate->hideInHierarchy || candidate == objectA.obj) continue;

                if (ImGui::Selectable(candidate->name.c_str()))
                {
                    pendingSelection = candidate;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();

            if (pendingSelection != nullptr) {
                PhysicsBody body = PhysicsBody();
                TransformComponent* tc = pendingSelection->GetComponent<TransformComponent>();
                RigidBodyComponent* pc = pendingSelection->GetComponent<RigidBodyComponent>();
                body.obj = pendingSelection;

                if (tc) {
                    body.position = &tc->worldPosition;
                    body.transformMatrix = &tc->WorldMatrix;
                    body.rotation = &tc->rotation;
                }
                if (pc) {
                    body.velocity = &pc->velocity;
                    body.angularVelocity = &pc->angularVelocity;
                    body.invInertia = &pc->inverseInertia;
                    body.invMass = &pc->inverseMass;
                }
                SetObjectB(body);
            }
        }
    }
    AttachPointWidget("B", objectB.obj, useCenterB, attachBEditing, attachPointB);

    ImGui::Spacing();

    ImGui::Text("Beta ");
    ImGui::SameLine();
    ImGui::DragFloat("##beta", &beta, 0.001f, 0.0f, 1.0f);

    ImGui::Text("Draw constraint ");
    ImGui::SameLine();
    ImGui::Checkbox("##Draw constraint", &canDrawConstraint);
}

void Constraint::CopyBaseFieldsFrom(const Constraint* src) {
    objectIdA = src->objectIdA;
    objectIdB = src->objectIdB;
    attachPointA = src->attachPointA;
    attachPointB = src->attachPointB;
	useCenterA = src->useCenterA;
    useCenterB = src->useCenterB;
    beta = src->beta;
    canDrawConstraint = src->canDrawConstraint;
    Name = src->Name;
}

void Constraint::Serialize(BinaryWriter& w) {
    w.WriteString(Name);
    w.Write(objectIdA);
    w.Write(objectIdB);
    w.Write(beta);
    w.Write(attachPointA);
    w.Write(attachPointB);
    w.Write(useCenterA);
	w.Write(useCenterB);
}

void Constraint::Deserialize(BinaryReader& r) {
    beta = r.Read<float>();
    attachPointA = r.Read<glm::vec3>();
    attachPointB = r.Read<glm::vec3>();
	useCenterA = r.Read<bool>();
    useCenterB = r.Read<bool>();
}