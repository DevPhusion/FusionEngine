#include "Constraint.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "MouseInteractComponent.h"
#include "ConstraintComponent.h"
#include "EditorManager.h"

Constraint::Constraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB)
{
    SetObjectA(objectA);
    SetObjectB(objectB);

    this->attachPointA = attachPointA;
    this->attachPointB = attachPointB;

    this->constraintDisplay = CreateConstraintDisplay();
    onPhysicsModeChangedCallbackID = EngineManager::getInstance().AddPhysicsModeChangedEvent([this] {OnPhysicsModeChanged();});
    ProcessConstraintDisplay();
}

Object* CreateAttachPointDisplay(const std::string& ownerName, const std::string& suffix,
    Object* targetObject, ObjectManager& om)
{
    constexpr float hs = 0.01f;

    std::vector<float> vertices = {
        -hs, -hs, 0.0f,  0.0f, 0.0f,
        +hs, -hs, 0.0f,  1.0f, 0.0f,
        +hs, +hs, 0.0f,  1.0f, 1.0f,
        -hs, +hs, 0.0f,  0.0f, 1.0f
    };

    auto display = std::make_unique<Object>(Shader("vertex.txt", "fragment.txt"));
    display->name = ownerName + " " + suffix + " Attach Display";
    display->AddComponent(std::make_unique<RenderComponent>(display.get(), vertices, display->shader, "red.jpg"));
    display->GetComponent<RenderComponent>()->z_index = 999;
    display->AddComponent(std::make_unique<TransformComponent>(
        display.get(), display->shader,
        display->GetComponent<RenderComponent>()->GetCenter()));
    display->AddComponent(std::make_unique<MouseInteractComponent>(display.get(), false));

    display->hidden = true;
    display->GetComponent<RenderComponent>()->Enabled = false;
    display->GetComponent<MouseInteractComponent>()->Enabled = false;
    display->GetComponent<MouseInteractComponent>()->Inspectable = false;

    display->GetComponent<TransformComponent>()->UpdateWorldPosition(
        targetObject->GetComponent<TransformComponent>()->GetWorldPosition());

    Object* raw = display.get();
    om.allObjects.push_back(std::move(display));
    return raw;
}

Object* Constraint::CreateConstraintDisplay()
{
    constexpr float hs = 0.01f;

    std::vector<float> vertices = {
        -hs, -hs, 0.0f,  0.0f, 0.0f,
        +hs, -hs, 0.0f,  1.0f, 0.0f,
        +hs, +hs, 0.0f,  1.0f, 1.0f,
        -hs, +hs, 0.0f,  0.0f, 1.0f
    };

    auto display = std::make_unique<Object>(Shader("vertex.txt", "fragment.txt"));
    display->hidden = true;
    display->name = Name + " Constraint display";
    display->AddComponent(std::make_unique<RenderComponent>(display.get(), vertices, display->shader, "floorTiled.png"));
    RenderComponent* rc = display->GetComponent<RenderComponent>();
    rc->z_index = -999;

    if (canDrawConstraint) {
        rc->SetEnabled(true);
    }
    else {
        rc->SetEnabled(false);
    }

    display->AddComponent(std::make_unique<TransformComponent>(
        display.get(), display->shader,
        rc->GetCenter()));

    Object* raw = display.get();
    ObjectManager::getInstance().allObjects.push_back(std::move(display));
    return raw;
}

void Constraint::EnsureDisplayA()
{
    if (attachDisplayA != nullptr || objectA.obj == nullptr) return;
    attachDisplayA = CreateAttachPointDisplay(Name, "A", objectA.obj,
        ObjectManager::getInstance());
    attachDisplayA->GetComponent<TransformComponent>()->AddTransformCallback(
        [this]() { OnDisplayAMoved(); });
    posSetA = false;
}

void Constraint::EnsureDisplayB()
{
    if (attachDisplayB != nullptr || objectB.obj == nullptr) return;
    attachDisplayB = CreateAttachPointDisplay(Name, "B", objectB.obj,
        ObjectManager::getInstance());
    attachDisplayB->GetComponent<TransformComponent>()->AddTransformCallback(
        [this]() { OnDisplayBMoved(); });
    posSetB = false;
}

void Constraint::DestroyDisplayA()
{
    if (attachDisplayA == nullptr) return;
    attachDisplayA->GetComponent<RenderComponent>()->Enabled = false;
    attachDisplayA->GetComponent<MouseInteractComponent>()->Enabled = false;
    ObjectManager::getInstance().RemoveObject(attachDisplayA);
    attachDisplayA = nullptr;
}

void Constraint::DestroyDisplayB()
{
    if (attachDisplayB == nullptr) return;
    attachDisplayB->GetComponent<RenderComponent>()->Enabled = false;
    attachDisplayB->GetComponent<MouseInteractComponent>()->Enabled = false;
    ObjectManager::getInstance().RemoveObject(attachDisplayB);
    attachDisplayB = nullptr;
}

void Constraint::OnObjectATransformChanged()
{
    if (objectA.obj == nullptr || attachDisplayA == nullptr) return;
    glm::vec3 world = objectA.obj->GetComponent<TransformComponent>()->ProjectToWorld(attachPointA);
    attachDisplayA->GetComponent<TransformComponent>()->UpdateWorldPosition(world);
    ProcessConstraintDisplay();
}

void Constraint::OnObjectBTransformChanged()
{
    if (objectB.obj == nullptr || attachDisplayB == nullptr) return;
    glm::vec3 world = objectB.obj->GetComponent<TransformComponent>()->ProjectToWorld(attachPointB);
    attachDisplayB->GetComponent<TransformComponent>()->UpdateWorldPosition(world);
    ProcessConstraintDisplay();
}

void Constraint::OnDisplayAMoved()
{
    if (objectA.obj == nullptr || attachDisplayA == nullptr) return;
    glm::vec3 world = attachDisplayA->GetComponent<TransformComponent>()->GetWorldPosition();
    attachPointA = objectA.obj->GetComponent<TransformComponent>()->ProjectToWorld(world, true);

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

    ProcessConstraintDisplay();
}

void Constraint::OnDisplayBMoved()
{
    if (objectB.obj == nullptr || attachDisplayB == nullptr) return;
    glm::vec3 world = attachDisplayB->GetComponent<TransformComponent>()->GetWorldPosition();
    attachPointB = objectB.obj->GetComponent<TransformComponent>()->ProjectToWorld(world, true);

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

    ProcessConstraintDisplay();
}

void Constraint::OnPhysicsModeChanged() {
    if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
        if (attachDisplayA) {
            attachDisplayA->GetComponent<RenderComponent>()->SetEnabled(false);
        }
        if (attachDisplayB) {
            attachDisplayB->GetComponent<RenderComponent>()->SetEnabled(false);
        }
    }
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
        objectA.obj->GetComponent<TransformComponent>()->RemoveTransformCallback(onTransformCallbackIdA);
        if (attachDisplayA) {
            attachDisplayA->GetComponent<RenderComponent>()->Enabled = false;
            attachDisplayA->GetComponent<MouseInteractComponent>()->Enabled = false;
        }
        if (virtualPMA) {
            objectA.obj->GetComponent<SoftBodyComponent>()->RemoveVirtualProxy(virtualPMA);
            virtualPMA = nullptr;
        }
    }

    objectA = obj;
    objectIdA = obj.obj ? obj.obj->id : 0;
    if (obj.obj == nullptr) return;


    onDeleteCallbackIdA = obj.obj->AddOnDeleteCallback([this]() { SetObjectA(PhysicsBody()); });
    onTransformCallbackIdA = obj.obj->GetComponent<TransformComponent>()->AddTransformCallback(
        [this]() { OnObjectATransformChanged(); });

    attachPointA = obj.obj->GetComponent<RenderComponent>()->GetCenter();
    EnsureDisplayA();
    posSetA = false;
}

void Constraint::SetObjectB(PhysicsBody obj)
{
    if (objectB.obj != nullptr)
    {
        RemoveMirrorFromObjectB();

        objectB.obj->RemoveOnDeleteCallback(onDeleteCallbackIdB);
        objectB.obj->GetComponent<TransformComponent>()->RemoveTransformCallback(onTransformCallbackIdB);
        if (attachDisplayB) {
            attachDisplayB->GetComponent<RenderComponent>()->Enabled = false;
            attachDisplayB->GetComponent<MouseInteractComponent>()->Enabled = false;
        }
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
    onTransformCallbackIdB = obj.obj->GetComponent<TransformComponent>()->AddTransformCallback(
        [this]() { OnObjectBTransformChanged(); });

    attachPointB = obj.obj->GetComponent<RenderComponent>()->GetCenter();
    EnsureDisplayB();
    posSetB = false;

    ProcessConstraintDisplay();
}

void Constraint::Unregister()
{
    RemoveMirrorFromObjectB();

    DestroyDisplayA();
    DestroyDisplayB();
    SetObjectA(PhysicsBody());
    SetObjectB(PhysicsBody());

    ObjectManager::getInstance().RemoveObject(constraintDisplay);
    EngineManager::getInstance().RemovePhysicsModeChangedEvent(onPhysicsModeChangedCallbackID);
    constraintDisplay = nullptr;
}

void Constraint::ProcessConstraintDisplay() {
    if (!constraintDisplay) return;
    RenderComponent* rc = constraintDisplay->GetComponent<RenderComponent>();
    if (rc) rc->SetEnabled(false);
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

    EnsureDisplayA();
    EnsureDisplayB();

    auto AttachPointWidget = [&](
        const char* popupId,
        Object* currentObj,
        bool& useCenter,
        Object*& display,
        bool& posSet)
        {
            if (display == nullptr) return;

            if (ImGui::Checkbox((std::string("Use Object Center##") + popupId).c_str(), &useCenter))
            {
                if (useCenter)
                {
                    display->GetComponent<TransformComponent>()->UpdateWorldPosition(
                        currentObj->GetComponent<TransformComponent>()->GetWorldPosition());
                    display->GetComponent<RenderComponent>()->Enabled = false;
                    display->GetComponent<MouseInteractComponent>()->Enabled = false;
                }
            }

            if (!useCenter)
            {
                bool displayVisible = display->GetComponent<RenderComponent>()->Enabled;

                if (!displayVisible)
                {
                    if (ImGui::Button((std::string("Change Attach Point##") + popupId).c_str()))
                    {
                        if (!posSet)
                        {
                            display->GetComponent<TransformComponent>()->UpdateWorldPosition(
                                currentObj->GetComponent<TransformComponent>()->GetWorldPosition());
                            posSet = true;
                        }
                        display->GetComponent<MouseInteractComponent>()->Enabled = true;
                        display->GetComponent<RenderComponent>()->Enabled = true;
                    }
                }
                else
                {
                    if (ImGui::Button((std::string("Confirm##") + popupId).c_str()))
                    {
                        display->GetComponent<MouseInteractComponent>()->Enabled = false;
                        display->GetComponent<RenderComponent>()->Enabled = false;
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
    AttachPointWidget("A", objectA.obj, useCenterA, attachDisplayA, posSetA);

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
                if (candidate->hidden || candidate == objectA.obj) continue;

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
    AttachPointWidget("B", objectB.obj, useCenterB, attachDisplayB, posSetB);

    ImGui::Spacing();

    ImGui::Text("Beta ");
    ImGui::SameLine();
    ImGui::DragFloat("##beta", &beta, 0.001f, 0.0f, 1.0f);

    ImGui::Text("Draw constraint ");
    ImGui::SameLine();
    if (ImGui::Checkbox("##Draw constraint", &canDrawConstraint)) {
        ProcessConstraintDisplay();
    }
}

void Constraint::CopyBaseFieldsFrom(const Constraint* src) {
    objectIdA = src->objectIdA;
    objectIdB = src->objectIdB;
    attachPointA = src->attachPointA;
    attachPointB = src->attachPointB;
    beta = src->beta;
    canDrawConstraint = src->canDrawConstraint;
    Name = src->Name;

    DestroyDisplayA();
    DestroyDisplayB();
    ObjectManager::getInstance().RemoveObject(constraintDisplay);
    constraintDisplay = nullptr;
}