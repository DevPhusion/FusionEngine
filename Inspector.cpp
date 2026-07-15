#include "Inspector.h"
#include "EditorManager.h"
#include "CollisionComponent.h"
#include "RigidBodyComponent.h"
#include "ConstraintComponent.h"
#include "SoftBodyComponent.h"
#include "FractureComponent.h"

Inspector::Inspector(std::string main) : EditorWindow(main) {

}


void Inspector::ProcessWindow() {
    if (hidden) return;

    ImGui::SetNextWindowPos(ImVec2(1510, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 880), ImGuiCond_FirstUseEver);

    ImGui::Begin(name.c_str());

    if (EditorManager::getInstance().selectedObject != nullptr) {
        Object* selected = EditorManager::getInstance().selectedObject;

        char objectNameBuffer[256];
#if defined(_MSC_VER)
        strcpy_s(objectNameBuffer, selected->name.c_str());
#else
        strncpy(objectNameBuffer, selected->name.c_str(), sizeof(objectNameBuffer) - 1);
#endif
        ImGui::Text("Name ");
        ImGui::SameLine();
        if (ImGui::InputText("##ObjectName", objectNameBuffer, 128)) {
            selected->name = std::string(objectNameBuffer);
            EngineManager::getInstance().EngineChangeEvent();
        }

        ImGui::Spacing();

        int pendingRemoval = -1;

        for (int i = 0; i < static_cast<int>(selected->components.size()); i++)
        {
            auto* component = selected->components[i].get();
            if (component->Hidden) continue;

            ImGui::PushID(i);

            const float removeButtonWidth = ImGui::CalcTextSize("×").x
                + ImGui::GetStyle().FramePadding.x * 2.0f;
            const float checkboxWidth = ImGui::GetFrameHeight();
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float availWidth = ImGui::GetContentRegionAvail().x;

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_AllowOverlap |
                ImGuiTreeNodeFlags_FramePadding |
                ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;

            bool nodeOpen = ImGui::TreeNodeEx("##compnode", flags, "%s", component->Name.c_str());

            if (component->CanDisable) {
                ImGui::SameLine(availWidth - removeButtonWidth - spacing - checkboxWidth);
                if (ImGui::Checkbox("##enabled", &component->Enabled))
                    component->SetEnabled(component->Enabled);
            }

            if (component->CanRemove) {
                ImGui::SameLine(availWidth - removeButtonWidth);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.05f, 0.05f, 1.0f));
                if (ImGui::SmallButton("×"))
                    pendingRemoval = i;
                ImGui::PopStyleColor(3);
            }

            if (nodeOpen) {
                ImGui::Indent();
                component->ProcessInspectorUI();
                ImGui::Unindent();
                ImGui::TreePop();
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (pendingRemoval != -1) {
            EngineManager::getInstance().EngineChangeEvent();
            selected->RemoveComponent(pendingRemoval);
        }

      
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const float buttonWidth = 180.0f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);
        if (ImGui::Button("+ Add Component", ImVec2(buttonWidth, 0)))
            ImGui::OpenPopup("Add Component");

        if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::SeparatorText("Component Type");

            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));

            ImGui::Spacing();

            std::string search = m_SearchBuffer;

            if (!selected->HasComponent<RigidBodyComponent>() && std::string("Rigid Body Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Rigid Body Component")) {
                    selected->AddComponent(std::make_unique<RigidBodyComponent>(selected));
                    EngineManager::getInstance().EngineChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<SoftBodyComponent>() && std::string("Soft Body Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Soft Body Component")) {
                    selected->AddComponent(std::make_unique<SoftBodyComponent>(selected));
                    EngineManager::getInstance().EngineChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<CollisionComponent>() && std::string("Collision Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Collision Component")) {
                    selected->AddComponent(std::make_unique<CollisionComponent>(selected));
                    EngineManager::getInstance().EngineChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<ConstraintComponent>() && std::string("Constraint Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Constraint Component")) {
                    selected->AddComponent(std::make_unique<ConstraintComponent>(selected));
                    EngineManager::getInstance().EngineChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<FractureComponent>() && std::string("Fracture Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Fracture Component")) {
                    selected->AddComponent(std::make_unique<FractureComponent>(selected));
                    EngineManager::getInstance().EngineChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_SearchBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 50.0f));
    ImGui::End();
}