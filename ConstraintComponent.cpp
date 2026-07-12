#include "ConstraintComponent.h"
#include <algorithm>

#include "RevoluteConstraint.h"
#include "DistanceConstraint.h"
#include "WeldConstraint.h"
#include "SpringConstraint.h"
#include "PrismaticConstraint.h"

ConstraintComponent::ConstraintComponent(Object* parent)
    : ComponentBase<ConstraintComponent>(parent)
{
    Name = "Constraint Component";
}

void ConstraintComponent::AddConstraint(std::shared_ptr<Constraint> constraint)
{
    if (!constraint) return;
    PhysicsEngine::getInstance().RegisterPGSConstraint(constraint.get());
    appliedConstraints.push_back(std::move(constraint));
}

void ConstraintComponent::RemoveConstraint(Constraint* constraint)
{
    auto it = std::find_if(appliedConstraints.begin(), appliedConstraints.end(),
        [constraint](const std::shared_ptr<Constraint>& c) { return c.get() == constraint; });

    if (it == appliedConstraints.end()) return;

    PhysicsEngine::getInstance().UnRegisterPGSConstraint(it->get());
    (*it)->Unregister(); 
    appliedConstraints.erase(it);
}

void ConstraintComponent::RemoveConstraint(std::size_t index)
{
    if (index >= appliedConstraints.size()) return;

    auto& c = appliedConstraints[static_cast<int>(index)];
    PhysicsEngine::getInstance().UnRegisterPGSConstraint(c.get());
    c->Unregister(); 
    appliedConstraints.erase(appliedConstraints.begin() + static_cast<std::ptrdiff_t>(index));
}

void ConstraintComponent::CopyTo(Object* other) {
    ConstraintComponent* target = other->GetComponent<ConstraintComponent>();
    if (!target) {
        other->AddComponent(std::make_unique<ConstraintComponent>(other));
        target = other->GetComponent<ConstraintComponent>();
    }
}

std::unique_ptr<Component> ConstraintComponent::Clone(Object* parent) {
    std::unique_ptr<ConstraintComponent> comp = std::make_unique<ConstraintComponent>(parent);
    comp->SetEnabled(false);
    return comp;
}

void ConstraintComponent::OnDelete()
{
    for (auto& c : appliedConstraints)
    {
        PhysicsEngine::getInstance().UnRegisterPGSConstraint(c.get());
        c->Unregister();
    }
    appliedConstraints.clear();
}

void ConstraintComponent::ProcessInspectorUI()
{
    ImGui::Text("Constraints");

    if (ImGui::Button("+ Add Constraint"))
        ImGui::OpenPopup("Add Constraint");

    if (ImGui::BeginPopupModal("Add Constraint", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::SeparatorText("Constraint Type");

        PhysicsBody body = PhysicsBody();
        TransformComponent* tc = parent->GetComponent<TransformComponent>();
        RigidBodyComponent* pc = parent->GetComponent<RigidBodyComponent>();
        SoftBodyComponent* sb = parent->GetComponent<SoftBodyComponent>();
        body.obj = parent;

        if (tc) {
            body.position = &tc->worldPosition;
            body.transformMatrix = &tc->WorldMatrix;
            body.rotation = &tc->rotation;
        }
        if (sb) {
            body.pm = &sb->CenterPM;
            body.position = &sb->CenterPM->worldPos;
            body.velocity = &sb->CenterPM->velocity;
            body.angularVelocity = &sb->CenterPM->angularVelocity;
            body.invInertia = &sb->CenterPM->InverseInertia;
            body.invMass = &sb->inverseMass;
        }
        if (pc) {
            body.velocity = &pc->velocity;
            body.angularVelocity = &pc->angularVelocity;
            body.invInertia = &pc->inverseInertia;
            body.invMass = &pc->inverseMass;
        }

        if (ImGui::MenuItem("Distance Constraint"))
        {
            AddConstraint(std::make_shared<DistanceConstraint>(body, PhysicsBody(),
                parent->GetComponent<RenderComponent>()->GetCenter(), glm::vec3(0.0f), 5.0f));
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Spring Constraint"))
        {
            AddConstraint(std::make_shared<SpringConstraint>(body, PhysicsBody(),
                parent->GetComponent<RenderComponent>()->GetCenter(), glm::vec3(0.0f), 5.0f, 15.0f, 7.0f));
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Revolute Constraint"))
        {
            AddConstraint(std::make_shared<RevoluteConstraint>(body, PhysicsBody(),
                parent->GetComponent<RenderComponent>()->GetCenter(), glm::vec3(0.0f)));
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Weld Constraint"))
        {
            AddConstraint(std::make_shared<WeldConstraint>(body, PhysicsBody(),
                parent->GetComponent<RenderComponent>()->GetCenter(), glm::vec3(0.0f), 0.0f));
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Prismatic Constraint")) {
            AddConstraint(std::make_shared<PrismaticConstraint>(body, PhysicsBody(),
                parent->GetComponent<RenderComponent>()->GetCenter(), glm::vec3(0.0f), glm::vec3(0.0f)));
            ImGui::CloseCurrentPopup();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Standard centered close button for modal feel
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (appliedConstraints.empty() && mirroredConstraints.empty())
    {
        ImGui::TextDisabled("  (no constraints)");
        return;
    }

    int pendingRemoval = -1;

    for (int i = 0; i < static_cast<int>(mirroredConstraints.size()); ++i)
    {
        Constraint* c = mirroredConstraints[i];
        ImGui::PushID(~i); 

        const float availWidth = ImGui::GetContentRegionAvail().x;

        bool nodeOpen = ImGui::TreeNodeEx("##mirrornode",
            ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding,
            "(linked) %s [%d]", c->Name.c_str(), i);

        if (nodeOpen)
        {
            ImGui::Indent();
            c->ProcessMirroredUI();
            ImGui::Unindent();
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    for (int i = 0; i < static_cast<int>(appliedConstraints.size()); ++i)
    {
        Constraint* c = appliedConstraints[i].get();

        ImGui::PushID(i);

        const float removeButtonWidth = ImGui::CalcTextSize("×").x
            + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float availWidth = ImGui::GetContentRegionAvail().x;

        bool nodeOpen = ImGui::TreeNodeEx("##node",
            ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen,
            "%s [%d]", c->Name.c_str(), i);

        ImGui::SameLine(availWidth - removeButtonWidth);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.05f, 0.05f, 1.0f));

        if (ImGui::SmallButton("×"))
            pendingRemoval = i;

        ImGui::PopStyleColor(3);

        if (nodeOpen)
        {
            ImGui::Indent();
            c->ProcessInspectorUI(parent);
            ImGui::Unindent();
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (pendingRemoval != -1)
        RemoveConstraint(static_cast<std::size_t>(pendingRemoval));
}