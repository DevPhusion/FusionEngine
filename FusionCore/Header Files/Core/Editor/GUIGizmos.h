#pragma once
#include "../../Objects/Object.h"
#include "../../Components/GUI/RectTransformComponent.h"

enum class RectHandle {
    None, Move,
    TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
};

class GUIGizmos
{
public:
    static GUIGizmos& getInstance() { static GUIGizmos inst; return inst; }

    void Draw(ImDrawList* dl, glm::vec2 origin, glm::vec2 size);

    static bool IsUIElement(Object* obj);

    bool isDragging = false;
private:
    glm::vec2 viewOrigin{ 0.0f };
    glm::vec2 viewSize{ 0.0f };
    int lastDrawFrame = -1000;
    glm::vec2 mouseScreen{ 0.0f };

    RectHandle activeHandle = RectHandle::None;
    Object* draggedObject = nullptr;
    RectTransformComponent* draggedRect = nullptr;
    glm::vec2 dragStartMouse{ 0.0f };
    glm::vec2 dragStartMin{ 0.0f };        
    glm::vec2 dragStartMax{ 0.0f };
    glm::vec2 dragStartOffsetMin{ 0.0f };
    glm::vec2 dragStartOffsetMax{ 0.0f };
    glm::vec2 dragAnchorMinPx{ 0.0f };     
    glm::vec2 dragAnchorMaxPx{ 0.0f };
    float dragScale = 1.0f;

    static constexpr float handleSize = 8.0f;   
    static constexpr float hitPadding = 4.0f;   

    GUIGizmos();

    void OnMouseButton(int button, int action, int mods);
    void OnCursorPosition(double xpos, double ypos);

    bool BeginDrag(Object* obj, RectHandle handle);
    void ApplyDrag();
    void EndDrag();

    bool ViewValid() const;
    bool InsideView(glm::vec2 p) const;
    bool GetScreenRect(RectTransformComponent* rt, glm::vec2& mn, glm::vec2& mx, float& scale) const;
    RectHandle HitTestHandles(glm::vec2 mn, glm::vec2 mx, glm::vec2 mouse) const;
    Object* PickElementAt(glm::vec2 mouse) const;
    void PickRecursive(Object* node, float scale, glm::vec2 mouse, Object*& best) const;
};

