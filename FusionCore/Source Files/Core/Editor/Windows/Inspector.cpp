#include "../../../../Header Files/Core/Editor/Windows/Inspector.h"
#include "../../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../../Header Files/Components/CollisionComponent.h"
#include "../../../../Header Files/Components/RigidBodyComponent.h"
#include "../../../../Header Files/Components/ConstraintComponent.h"
#include "../../../../Header Files/Components/SoftBodyComponent.h"
#include "../../../../Header Files/Components/FractureComponent.h"
#include "../../../../Header Files/Components/FluidComponent.h"
#include "../../../../Header Files/Components/CameraComponent.h"
#include "../../../../Header Files/Components/ScriptComponent.h"


namespace {
    static ImU32 EyeIconColor(bool isHidden) {
        if (isHidden) return IM_COL32(128, 128, 128, 255);
        return IM_COL32(255, 255, 255, 255);
    }

    static void DrawEyeIcon(ImDrawList* drawList, ImVec2 center, float size, bool isHidden) {
        ImU32 color = EyeIconColor(isHidden);
        ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);

        float rx = size * 0.52f;
        float ry = size * 0.33f;

        const int segments = 24;
        ImVec2 points[segments];
        for (int i = 0; i < segments; i++) {
            float t = (2.0f * std::numbers::pi * i) / segments;
            points[i] = ImVec2(center.x + rx * cosf(t), center.y + ry * sinf(t));
        }
        drawList->AddConvexPolyFilled(points, segments, color);

        float ringOuterR = ry * 0.62f;
        float ringInnerR = ry * 0.30f;
        drawList->AddCircleFilled(center, ringOuterR, bgColor, 16);
        drawList->AddCircleFilled(center, ringInnerR, color, 16);

        if (isHidden) {
            float lineHalf = size * 0.58f;
            ImVec2 dir(0.82f, 0.57f);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            dir.x /= len; dir.y /= len;
            ImVec2 p0(center.x - dir.x * lineHalf, center.y + dir.y * lineHalf);
            ImVec2 p1(center.x + dir.x * lineHalf, center.y - dir.y * lineHalf);
            drawList->AddLine(p0, p1, color, size * 0.14f);
        }
    }

    static bool DrawEyeToggleButton(const char* strId, bool isHidden, float size) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(strId, ImVec2(size, size));
        bool clicked = ImGui::IsItemClicked();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
        DrawEyeIcon(drawList, center, size, isHidden);

        return clicked;
    }
}


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
        objectNameBuffer[sizeof(objectNameBuffer) - 1] = '\0';
#endif
        ImGui::Text("Name ");
        ImGui::SameLine();
        if (ImGui::InputText("##ObjectName", objectNameBuffer, sizeof(objectNameBuffer))) {
            selected->name = std::string(objectNameBuffer);
            EngineManager::getInstance().SceneChangeEvent();
        }

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string desiredName = selected->name.empty() ? "Object" : selected->name;
            selected->name = ObjectManager::getInstance().GenerateUniqueName(desiredName, selected);
            EngineManager::getInstance().SceneChangeEvent();
        }

        ImGui::SameLine();
        float eyeIconSize = ImGui::GetFrameHeight() * 1.0f;
        if (DrawEyeToggleButton("##InspectorEyeToggle", selected->hidden, eyeIconSize)) {
            if (selected->hidden) selected->Show();
            else selected->Hide();
            EngineManager::getInstance().SceneChangeEvent();
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

            std::string displayName = component->Name;
            ScriptComponent* script = dynamic_cast<ScriptComponent*>(component);
            if (script) {
                displayName = script->GetDisplayName();
                if (displayName == "") displayName = "Unknown script";
            }

            bool nodeOpen = ImGui::TreeNodeEx("##compnode", flags, "%s", displayName.c_str());

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
            EngineManager::getInstance().SceneChangeEvent();
            EditorManager::getInstance().BeginEdit({ selected });
            selected->RemoveComponent(pendingRemoval);
            EditorManager::getInstance().EndEdit({ selected });

        }

      
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const float buttonWidth = 180.0f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);
        if (ImGui::Button("+ Add Component", ImVec2(buttonWidth, 0)))
            ImGui::OpenPopup("Add Component");

        if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));

            ImGui::SeparatorText("Components");

            std::string search = m_SearchBuffer;

            if (!selected->HasComponent<RenderComponent>() && std::string("Render Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Render Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<RenderComponent>(selected, std::vector<float> {}, selected->shader, ""));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            if (!selected->HasComponent<CameraComponent>() && std::string("Camera Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Camera Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<CameraComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            if (!selected->HasComponent<RigidBodyComponent>() && std::string("Rigid Body Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Rigid Body Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<RigidBodyComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<SoftBodyComponent>() && std::string("Soft Body Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Soft Body Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<SoftBodyComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<CollisionComponent>() && std::string("Collision Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Collision Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<CollisionComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<ConstraintComponent>() && std::string("Constraint Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Constraint Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<ConstraintComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            if (!selected->HasComponent<FractureComponent>() && std::string("Fracture Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Fracture Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<FractureComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            if (!selected->HasComponent<FluidComponent>() && std::string("Fluid Component").find(search) != std::string::npos)
                if (ImGui::MenuItem("Fluid Component")) {
                    EditorManager::getInstance().BeginEdit({ selected });
                    selected->AddComponent(std::make_unique<FluidComponent>(selected));
                    EditorManager::getInstance().EndEdit({ selected });
                    EngineManager::getInstance().SceneChangeEvent();
                    m_SearchBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

            const auto& registeredScripts = ScriptManager::getInstance().registeredScripts;
            if (!registeredScripts.empty()) {
                ImGui::Spacing();
                ImGui::SeparatorText("Scripts");

                auto hasScript = [&](const std::string& scriptVirtualPath) {
                    for (auto& comp : selected->components) {
                        ScriptComponent* sc = dynamic_cast<ScriptComponent*>(comp.get());
                        if (sc && sc->sourcePath == scriptVirtualPath)
                            return true;
                    }
                    return false;
                    };

                for (const std::string& scriptPath : registeredScripts) {
                    if (hasScript(scriptPath))
                        continue;

                    std::string scriptDisplayName = std::filesystem::path(scriptPath).stem().string();
                    if (scriptDisplayName.find(search) == std::string::npos)
                        continue;

                    ImGui::PushID(scriptPath.c_str());
                    if (ImGui::MenuItem(scriptDisplayName.c_str())) {
                        EditorManager::getInstance().BeginEdit({ selected });
                        selected->AddComponent(std::make_unique<ScriptComponent>(selected, scriptPath));
                        EditorManager::getInstance().EndEdit({ selected });
                        EngineManager::getInstance().SceneChangeEvent();
                        m_SearchBuffer[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
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