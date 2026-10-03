#include "../../../../Header Files/Core/Editor/Windows/Inspector.h"
#include "../../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../../Header Files/Core/Editor/EditorTheme.h"
#include "../../../../Header Files/Core/Editor/EditorField.h"
#include "../../../../Header Files/Components/Components.h"
#include <numbers>
#include <cctype>
#include <functional>
#include <algorithm>
#include <filesystem>

namespace {
    static ImU32 EyeIconColor(bool isHidden) {
        return ImGui::GetColorU32(isHidden ? ImGuiCol_TextDisabled : ImGuiCol_Text);
    }

    static void DrawEyeIcon(ImDrawList* drawList, ImVec2 center, float size, bool isHidden) {
        ImU32 color = EyeIconColor(isHidden);
        ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_FrameBg);

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
        if (ImGui::IsItemHovered())
            drawList->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
                ImGui::GetColorU32(ImGuiCol_FrameBgHovered), ImGui::GetStyle().FrameRounding);

        ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
        DrawEyeIcon(drawList, center, size * 0.9f, isHidden);

        return clicked;
    }

    static bool ContainsNoCase(const char* haystack, const char* needle) {
        std::string h = haystack, n = needle;
        std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return h.find(n) != std::string::npos;
    }

    static std::string ToLower(std::string v) {
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return v;
    }

    static bool StartsWithNoCase(const std::string& text, const std::string& prefix) {
        return ToLower(text).rfind(ToLower(prefix), 0) == 0;
    }

    static void FinishAdd(char* searchBuf) {
        EngineManager::getInstance().SceneChangeEvent();
        searchBuf[0] = '\0';
        ImGui::CloseCurrentPopup();
    }

    template<typename T, typename... Args>
    static void AddComponentTo(Object* o, Args&&... args) {
        EditorManager::getInstance().BeginEdit({ o });
        auto comp = std::make_unique<T>(o, std::forward<Args>(args)...);
        T* raw = comp.get();
        o->AddComponent(std::move(comp));
        raw->Activate();
        EditorManager::getInstance().EndEdit({ o });
    }

    struct ComponentEntry {
        std::string label;
        std::function<bool(Object*)> available;
        std::function<void(Object*)> add;
    };

    template<typename T>
    static ComponentEntry SimpleEntry(const char* label) {
        return { label,
            [](Object* o) { return !o->HasComponent<T>(); },
            [](Object* o) { AddComponentTo<T>(o); } };
    }

    static const std::vector<ComponentEntry>& ComponentRegistry() {
        static const std::vector<ComponentEntry> entries = {
            { "Render Component",
              [](Object* o) { return !o->HasComponent<RenderComponent>(); },
              [](Object* o) { AddComponentTo<RenderComponent>(o, std::vector<float>{}, o->shader, ""); } },
            SimpleEntry<CameraComponent>("Camera Component"),
            SimpleEntry<AudioComponent>("Audio Component"),
            SimpleEntry<RigidBodyComponent>("Rigid Body Component"),
            SimpleEntry<SoftBodyComponent>("Soft Body Component"),
            SimpleEntry<CollisionComponent>("Collision Component"),
            SimpleEntry<ConstraintComponent>("Constraint Component"),
            SimpleEntry<FractureComponent>("Fracture Component"),
            SimpleEntry<FluidComponent>("Fluid Component"),
            SimpleEntry<GasComponent>("Gas Component"),
            { "Agent Component",
              [](Object* o) { return !o->HasComponent<AgentComponent>() && PackageManager::getInstance().IsPackageInstalled("rl"); },
              [](Object* o) { AddComponentTo<AgentComponent>(o); } },
        };
        return entries;
    }

    struct Candidate {
        std::string key;
        std::string label;
        std::string subtitle;
        std::function<void()> add;
    };

    static std::vector<std::string> s_recentAdds;
    static constexpr size_t kMaxRecents = 5;

    static void PushRecent(const std::string& key) {
        s_recentAdds.erase(std::remove(s_recentAdds.begin(), s_recentAdds.end(), key), s_recentAdds.end());
        s_recentAdds.insert(s_recentAdds.begin(), key);
        if (s_recentAdds.size() > kMaxRecents) s_recentAdds.resize(kMaxRecents);
    }

    static bool DrawCandidateRow(const Candidate& c) {
        ImGui::PushID(c.key.c_str());
        bool clicked = ImGui::Selectable("##row");
        ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddText(min, ImGui::GetColorU32(ImGuiCol_Text), c.label.c_str());
        if (!c.subtitle.empty()) {
            float x = min.x + ImGui::CalcTextSize(c.label.c_str()).x + ImGui::GetStyle().ItemSpacing.x;
            std::string sub = "(" + c.subtitle + ")";
            dl->AddText(ImVec2(x, min.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str());
        }
        ImGui::PopID();
        return clicked;
    }
}

Inspector::Inspector(std::string name) : EditorWindow(name) {

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
        const float eyeIconSize = ImGui::GetFrameHeight();
        const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;

        bool boldPushed = EditorTheme::PushBold();
        ImGui::SetNextItemWidth(-(eyeIconSize + itemSpacing));
        if (ImGui::InputTextWithHint("##ObjectName", "Object name", objectNameBuffer, sizeof(objectNameBuffer))) {
            selected->name = std::string(objectNameBuffer);
            EngineManager::getInstance().SceneChangeEvent();
        }
        EditorTheme::PopBold(boldPushed);

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string desiredName = selected->name.empty() ? "Object" : selected->name;
            selected->name = ObjectManager::getInstance().GenerateUniqueName(desiredName, selected);
            EngineManager::getInstance().SceneChangeEvent();
        }

        ImGui::SameLine();
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

            const float removeButtonWidth = ImGui::GetFrameHeight();
            const float checkboxWidth = ImGui::GetFrameHeight();
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float availWidth = ImGui::GetContentRegionAvail().x;

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_AllowOverlap |
                ImGuiTreeNodeFlags_FramePadding |
                ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_Framed;

            std::string displayName = component->Name;
            ScriptComponent* script = dynamic_cast<ScriptComponent*>(component);
            if (script) {
                displayName = script->GetDisplayName();
                if (displayName == "") displayName = "Unknown script";
            }

            bool headerBold = EditorTheme::PushBold();
            bool nodeOpen = ImGui::TreeNodeEx("##compnode", flags, "%s", displayName.c_str());
            EditorTheme::PopBold(headerBold);

            {
                ImVec2 mn = ImGui::GetItemRectMin();
                ImVec2 mx = ImGui::GetItemRectMax();
                ImU32 strip = ImGui::GetColorU32(
                    (component->CanDisable && !component->Enabled) ? ImVec4(0.30f, 0.30f, 0.30f, 1.0f) : EditorTheme::Accent());
                ImGui::GetWindowDrawList()->AddRectFilled(mn, ImVec2(mn.x + 3.0f, mx.y), strip,
                    ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersLeft);
            }

            if (component->CanDisable) {
                ImGui::SameLine(availWidth - removeButtonWidth - spacing - checkboxWidth);
                EditorField::CheckboxScene(selected, nullptr, "##enabled", &component->Enabled, [&] {
                    component->SetEnabled(component->Enabled);
                    });
            }

            if (component->CanRemove) {
                ImGui::SameLine(availWidth - removeButtonWidth);
                if (EditorTheme::CloseButton("##remove"))
                    pendingRemoval = i;
            }

            if (nodeOpen) {
                ImGui::Spacing();
                ImGui::Indent();
                component->ProcessInspectorUI();
                ImGui::Unindent();
                ImGui::Spacing();
                ImGui::TreePop();
            }

            ImGui::PopID();
            ImGui::Spacing();
        }

        if (pendingRemoval != -1) {
            EditorManager::getInstance().BeginEdit({ selected });
            selected->RemoveComponent(pendingRemoval);
            EditorManager::getInstance().EndEdit({ selected });
            EngineManager::getInstance().SceneChangeEvent();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const float buttonWidth = 200.0f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f + ImGui::GetCursorPosX());
        bool addBold = EditorTheme::PushBold();
        bool openAdd = ImGui::Button("+  Add Component", ImVec2(buttonWidth, 0));
        EditorTheme::PopBold(addBold);
        const ImVec2 buttonMin = ImGui::GetItemRectMin();
        const ImVec2 buttonMax = ImGui::GetItemRectMax();
        if (openAdd)
            ImGui::OpenPopup("Add Component");

        ImGui::SetNextWindowPos(ImVec2(buttonMin.x - 80.0f, buttonMin.y - 4.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
        if (ImGui::BeginPopup("Add Component", ImGuiWindowFlags_NoSavedSettings))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            ImGui::SetNextItemWidth(-1);
            bool enterPressed = ImGui::InputTextWithHint("##search", "Search components and scripts...",
                m_SearchBuffer, sizeof(m_SearchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
            const std::string search = m_SearchBuffer;

            ImGui::Spacing();

            std::vector<Candidate> all;
            for (const ComponentEntry& e : ComponentRegistry()) {
                if (!e.available(selected)) continue;
                const ComponentEntry* ep = &e;
                all.push_back({ "c:" + e.label, e.label, "", [ep, selected] { ep->add(selected); } });
            }

            for (const std::string& scriptPath : ScriptManager::getInstance().registeredScripts) {
                bool alreadyAttached = false;
                for (auto& comp : selected->components) {
                    ScriptComponent* sc = dynamic_cast<ScriptComponent*>(comp.get());
                    if (sc && sc->sourcePath == scriptPath) { alreadyAttached = true; break; }
                }
                if (alreadyAttached) continue;

                std::string relative = scriptPath;
                if (relative.rfind("res://", 0) == 0) relative.erase(0, 6);

                all.push_back({ "s:" + scriptPath,
                    std::filesystem::path(scriptPath).stem().string(),
                    relative,
                    [selected, scriptPath] { AddComponentTo<ScriptComponent>(selected, scriptPath); } });
            }

            std::vector<const Candidate*> shown;
            if (search.empty()) {
                for (const std::string& key : s_recentAdds)
                    for (const Candidate& c : all)
                        if (c.key == key) { shown.push_back(&c); break; }
            }
            else {
                for (const Candidate& c : all)
                    if (ContainsNoCase(c.label.c_str(), search.c_str()) ||
                        (!c.subtitle.empty() && ContainsNoCase(c.subtitle.c_str(), search.c_str())))
                        shown.push_back(&c);

                std::stable_partition(shown.begin(), shown.end(),
                    [&](const Candidate* c) { return StartsWithNoCase(c->label, search); });
            }

            const Candidate* picked = nullptr;
            if (shown.empty()) {
                ImGui::TextDisabled(search.empty() ? "Type to search components and scripts" : "No results");
            }
            else {
                for (const Candidate* c : shown)
                    if (DrawCandidateRow(*c)) picked = c;
                if (enterPressed) picked = shown[0];
            }

            if (picked) {
                const std::string key = picked->key;
                picked->add();
                PushRecent(key);
                FinishAdd(m_SearchBuffer);
            }

            ImGui::EndPopup();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 50.0f));
    ImGui::End();
}