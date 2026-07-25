#include "../../Header Files/Components/CollisionComponent.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"

CollisionComponent::CollisionComponent(Object* parent) : ComponentBase<CollisionComponent>(parent) {
	Name = "Collision Component";
	calculateBoundingCircle();
	onTransformCallbackID = parent->GetComponent<TransformComponent>()->AddTransformCallback([this]() {this->calculateBoundingCircle();});
    FluidComponent* fc = parent->GetComponent<FluidComponent>();
    if (!fc) {
        BAHnode = PhysicsEngine::getInstance().RegisterBoundingAreaNode(parent, boundingCircle);
    }
}

void CollisionComponent::DrawLayerMaskUI(const char* label, uint16_t* layer) {
    if (!layer) return;

    ImGui::Text("%s", label);
    ImGui::BeginGroup();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

    // Capture start position AFTER pushing style vars
    ImVec2 startPos = ImGui::GetCursorPos();

    const float btnSize = 24.0f;
    const float gap = 2.0f;


    for (int i = 0; i < 16; ++i) {
        int col = i % 8;
        int row = i / 8;
        ImGui::SetCursorPos(ImVec2(
            startPos.x + col * (btnSize + gap),
            startPos.y + row * (btnSize + gap)
        ));


        bool is_set = (*layer & (1 << i)) != 0;
        if (is_set)
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

        ImGui::PushID(label);
        ImGui::PushID(i);

        if (ImGui::Button(std::to_string(i + 1).c_str(), ImVec2(btnSize, btnSize))) {
            *layer ^= (1 << i);
            boundingCircle.collisionLayer = collisionLayer;
            boundingCircle.collisionMask = collisionMask;
            BAHnode->area.collisionLayer = collisionLayer;
            BAHnode->area.collisionMask = collisionMask;
            FluidComponent* fc = parent->GetComponent<FluidComponent>();
            if (fc) fc->UpdateCollisionLayerMask();
        }

        ImGui::PopID();
        ImGui::PopID();

        if (is_set)
            ImGui::PopStyleColor();
    }

    // Manually advance cursor past both rows so layout below is correct
    ImGui::SetCursorPos(ImVec2(startPos.x, startPos.y + 2 * (btnSize + gap)));

    ImGui::Dummy(ImVec2(8 * (btnSize + gap) - gap, 0)); // set group width


    ImGui::PopStyleVar(2);
    ImGui::EndGroup();

}

void CollisionComponent::CopyTo(Object* other) {
    CollisionComponent* target = other->GetComponent<CollisionComponent>();
    if (!target) {
        other->AddComponent(std::make_unique<CollisionComponent>(other));
        target = other->GetComponent<CollisionComponent>();
    }

    target->boundingCircle = boundingCircle;
    target->collisionLayer = collisionLayer;
    target->collisionMask = collisionMask;
    target->calculateBoundingCircle();
    FluidComponent* fc = other->GetComponent<FluidComponent>();
    if (fc) fc->UpdateCollisionLayerMask();
}

std::unique_ptr<Component> CollisionComponent::Clone(Object* parent) {
    std::unique_ptr<CollisionComponent> comp = std::make_unique<CollisionComponent>(parent);
    comp->boundingCircle = boundingCircle;
    comp->collisionLayer = collisionLayer;
    comp->collisionMask = collisionMask;
    comp->calculateBoundingCircle();
    comp->SetEnabled(false);
    return comp;
}

void CollisionComponent::Serialize(BinaryWriter& w) {
    Component::Serialize(w);

    w.Write(collisionLayer);
    w.Write(collisionMask);
}
void CollisionComponent::Deserialize(BinaryReader& r) {
    Component::Deserialize(r);
    collisionLayer = r.Read<uint16_t>();
    collisionMask = r.Read<uint16_t>();
    calculateBoundingCircle();
}

void CollisionComponent::PostLoad() {
    FluidComponent* fc = parent->GetComponent<FluidComponent>();
    if (fc) {
        fc->UpdateCollisionLayerMask();
        if (BAHnode) {
            TransformComponent* tc = parent->GetComponent<TransformComponent>();
            if (tc) tc->RemoveTransformCallback(onTransformCallbackID);
            PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
        }
    }
}

void CollisionComponent::ProcessInspectorUI() {
    DrawLayerMaskUI("Layer", &collisionLayer);
    ImGui::Spacing();
    DrawLayerMaskUI("Mask", &collisionMask);
}

void CollisionComponent::SetEnabled(bool enabled) {
    if (enabled) {
        if (!BAHnode) {
            BAHnode = PhysicsEngine::getInstance().RegisterBoundingAreaNode(parent, boundingCircle);
        }
    }
    else {
        PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
        BAHnode = nullptr;
    }
}

void CollisionComponent::OnDelete() {
    TransformComponent* tc = parent->GetComponent<TransformComponent>();
    if (tc) tc->RemoveTransformCallback(onTransformCallbackID);
	PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
}

void CollisionComponent::calculateBoundingCircle() {
    EngineManager::getInstance().EngineChangeEvent();
    std::vector<std::vector<float>> points = parent->GetComponent<RenderComponent>()->points;
    TransformComponent* tc = parent->GetComponent<TransformComponent>();
    glm::vec3 center = tc->GetWorldPosition();

    float radius = 0;
    for (int i = 0; i < points.size(); i++) {
        glm::vec3 p = glm::vec3(points[i][0], points[i][1], 0);
        glm::vec3 worldP = tc->ProjectToWorld(p);
        float dist = glm::distance(center, worldP);
        if (dist > radius) radius = dist;
    }

    boundingCircle.center = center;
    boundingCircle.radius = radius;
    boundingCircle.collisionLayer = collisionLayer;  
    boundingCircle.collisionMask = collisionMask;

    BAHnode = PhysicsEngine::getInstance().root.searchFor(parent);
    if (BAHnode != nullptr) {
        BAHnode->area = boundingCircle;
        BAHnode->recalculateBoundingArea();
        if (BAHnode->parent != nullptr)
            BAHnode->parent->recalculateBoundingArea();
    }
}
