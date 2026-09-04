#pragma once
#include "Component.h"
#include "../Core/InputManager.h"
#include "../Objects/Object.h"
#include "SoftBodyComponent.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "EditorRenderComponent.h"
#include "../Core/EngineManager.h"
#include "../Core/Physics/PhysicsEngine.h"

class MouseInteractComponent : public ComponentBase<MouseInteractComponent>
{
public:
    MouseInteractComponent(Object* parent);
    MouseInteractComponent() = default;

    static bool ObjectSelected; //prevent multiple selection;
    bool Selected;
    bool Inspectable = true;

    virtual void Activate();
    virtual void Deactivate();
    virtual void ProcessInspectorUI();
    virtual void OnDelete();
    virtual void CopyTo(Object* other);
    virtual void Serialize(BinaryWriter& w);
    virtual void Deserialize(BinaryReader& r);

    void RegisterCallbacks();
    void UnregisterCallbacks();
    void FindSelectedPolygon(int button, int action, int mods);
    void DragPolygon(double xpos, double ypos);
    void SetSelectedPolygon(Object* obj, bool enable);
    void OnPhysicsModeChanged();
private:
    bool isEditingViaMouse = false;
    std::vector<int> mouseButtonCallbackID;
    std::vector<int> cursorPosCallbackID;
    int physicsModeChangedCallbackID;
};