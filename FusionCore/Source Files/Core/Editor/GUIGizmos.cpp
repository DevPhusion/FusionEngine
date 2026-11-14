#include "../../../Header Files/Core/Editor/GUIGizmos.h"
#include "../../../Header Files/Core/EngineManager.h"
#include "../../../Header Files/Core/GUIEngine.h"
#include "../../../Header Files/Components/MouseInteractComponent.h"
#include "../../../Header Files/Components/GUI/CanvasComponent.h"


namespace {
	struct HandleDef { RectHandle id; glm::vec2 pos; };

	std::array<HandleDef, 8> GetHandles(glm::vec2 mn, glm::vec2 mx) {
		const float cx = (mn.x + mx.x) * 0.5f;
		const float cy = (mn.y + mx.y) * 0.5f;
		return { {
			{ RectHandle::TopLeft,     { mn.x, mn.y } },
			{ RectHandle::Top,         { cx,   mn.y } },
			{ RectHandle::TopRight,    { mx.x, mn.y } },
			{ RectHandle::Right,       { mx.x, cy   } },
			{ RectHandle::BottomRight, { mx.x, mx.y } },
			{ RectHandle::Bottom,      { cx,   mx.y } },
			{ RectHandle::BottomLeft,  { mn.x, mx.y } },
			{ RectHandle::Left,        { mn.x, cy   } },
		} };
	}

	void EdgeFlags(RectHandle h, bool& l, bool& r, bool& t, bool& b) {
		l = h == RectHandle::TopLeft || h == RectHandle::Left || h == RectHandle::BottomLeft;
		r = h == RectHandle::TopRight || h == RectHandle::Right || h == RectHandle::BottomRight;
		t = h == RectHandle::TopLeft || h == RectHandle::Top || h == RectHandle::TopRight;
		b = h == RectHandle::BottomLeft || h == RectHandle::Bottom || h == RectHandle::BottomRight;
	}

	ImGuiMouseCursor CursorFor(RectHandle h) {
		switch (h) {
		case RectHandle::TopLeft:
		case RectHandle::BottomRight: return ImGuiMouseCursor_ResizeNWSE;
		case RectHandle::TopRight:
		case RectHandle::BottomLeft:  return ImGuiMouseCursor_ResizeNESW;
		case RectHandle::Top:
		case RectHandle::Bottom:      return ImGuiMouseCursor_ResizeNS;
		case RectHandle::Left:
		case RectHandle::Right:       return ImGuiMouseCursor_ResizeEW;
		case RectHandle::Move:        return ImGuiMouseCursor_ResizeAll;
		default:                      return ImGuiMouseCursor_Arrow;
		}
	}

	bool PointInRect(glm::vec2 p, glm::vec2 mn, glm::vec2 mx) {
		return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y;
	}

	bool EditingAllowed() {
		EngineManager& em = EngineManager::getInstance();
		return !em.isPlayer
			&& em.EnginePhysicsMode != EngineManager::PhysicsMode::Simulate
			&& em.EngineInteractMode == EngineManager::InteractMode::EditorSelect;
	}
}

GUIGizmos::GUIGizmos() {
	InputManager::getInstance().SetMouseButtonCallback(
		[this](int button, int action, int mods) { OnMouseButton(button, action, mods); }, 1000);
	InputManager::getInstance().SetCursorPositionCallback(
		[this](double xpos, double ypos) { OnCursorPosition(xpos, ypos); }, 1000);
}

bool GUIGizmos::IsUIElement(Object* obj) {
	return obj && obj->HasComponent<RectTransformComponent>() && !obj->HasComponent<CanvasComponent>();
}

bool GUIGizmos::ViewValid() const {
	if (!ImGui::GetCurrentContext()) return false;
	return ImGui::GetFrameCount() - lastDrawFrame <= 2;   
}

bool GUIGizmos::InsideView(glm::vec2 p) const {
	return p.x >= viewOrigin.x && p.y >= viewOrigin.y
		&& p.x < viewOrigin.x + viewSize.x && p.y < viewOrigin.y + viewSize.y;
}

bool GUIGizmos::GetScreenRect(RectTransformComponent* rt, glm::vec2& mn, glm::vec2& mx, float& scale) const {
	CanvasComponent* canvas = rt->FindCanvas();
	if (!canvas || canvas->renderMode != CanvasComponent::RenderMode::ScreenSpace) return false;

	scale = canvas->GetScaleFactor();
	const UiRect r = rt->GetRect();
	mn = viewOrigin + r.pos * scale;
	mx = mn + r.size * scale;
	return true;
}

RectHandle GUIGizmos::HitTestHandles(glm::vec2 mn, glm::vec2 mx, glm::vec2 mouse) const {
	const float reach = handleSize * 0.5f + hitPadding;
	RectHandle best = RectHandle::None;
	float bestDist = 1e9f;

	for (const HandleDef& h : GetHandles(mn, mx)) {
		glm::vec2 d = glm::abs(mouse - h.pos);
		if (d.x > reach || d.y > reach) continue;
		float dist = d.x * d.x + d.y * d.y;
		if (dist < bestDist) { bestDist = dist; best = h.id; }
	}
	return best;
}

void GUIGizmos::PickRecursive(Object* node, float scale, glm::vec2 mouse, Object*& best) const {
	for (Object* child : node->children) {
		RectTransformComponent* rt = child->GetComponent<RectTransformComponent>();
		if (!rt || child->hidden) continue;

		if (GUIEngine::getInstance().HasDrawable(child)) {
			const UiRect r = rt->GetRect();
			glm::vec2 mn = viewOrigin + r.pos * scale;
			glm::vec2 mx = mn + r.size * scale;
			if (PointInRect(mouse, mn, mx)) best = child;
		}
		PickRecursive(child, scale, mouse, best);
	}
}

Object* GUIGizmos::PickElementAt(glm::vec2 mouse) const {
	Object* best = nullptr;
	for (CanvasComponent* canvas : CanvasComponent::GetActiveCanvases()) {   // sorted by sortOrder
		if (canvas->renderMode != CanvasComponent::RenderMode::ScreenSpace) continue;
		if (!canvas->parent) continue;
		PickRecursive(canvas->parent, canvas->GetScaleFactor(), mouse, best);
	}
	return best;
}

void GUIGizmos::Draw(ImDrawList* dl, glm::vec2 origin, glm::vec2 size) {
	viewOrigin = origin;
	viewSize = size;
	lastDrawFrame = ImGui::GetFrameCount();

	if (!dl || !EditingAllowed()) return;

	Object* sel = EditorManager::getInstance().selectedObject;
	if (!IsUIElement(sel)) return;

	RectTransformComponent* rt = sel->GetComponent<RectTransformComponent>();
	glm::vec2 mn, mx;
	float scale;
	if (!GetScreenRect(rt, mn, mx, scale)) return;

	RectHandle hot = isDragging ? activeHandle : RectHandle::None;
	if (!isDragging) {
		ImVec2 mp = ImGui::GetIO().MousePos;
		glm::vec2 m(mp.x, mp.y);
		if (InsideView(m)) hot = HitTestHandles(mn, mx, m);
	}
	if (hot != RectHandle::None) ImGui::SetMouseCursor(CursorFor(hot));

	const ImU32 outline = IM_COL32(0, 170, 255, 255);
	const ImU32 fill = IM_COL32(255, 255, 255, 255);
	const ImU32 fillHot = IM_COL32(255, 210, 60, 255);

	dl->AddRect(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), outline, 0.0f, 0, 1.5f);

	const float half = handleSize * 0.5f;
	for (const HandleDef& h : GetHandles(mn, mx)) {
		ImVec2 a(h.pos.x - half, h.pos.y - half);
		ImVec2 b(h.pos.x + half, h.pos.y + half);
		dl->AddRectFilled(a, b, hot == h.id ? fillHot : fill);
		dl->AddRect(a, b, outline, 0.0f, 0, 1.5f);
	}
}

bool GUIGizmos::BeginDrag(Object* obj, RectHandle handle) {
	RectTransformComponent* rt = obj->GetComponent<RectTransformComponent>();
	if (!rt) return false;
	CanvasComponent* canvas = rt->FindCanvas();
	if (!canvas) return false;

	const UiRect r = rt->GetRect();
	const UiRect parentRect = rt->GetParentRect();

	draggedObject = obj;
	draggedRect = rt;
	activeHandle = handle;
	isDragging = true;

	dragScale = std::max(canvas->GetScaleFactor(), 1e-4f);
	dragStartMouse = mouseScreen;
	dragStartMin = r.pos;
	dragStartMax = r.pos + r.size;
	dragStartOffsetMin = rt->offsetMin;
	dragStartOffsetMax = rt->offsetMax;
	dragAnchorMinPx = parentRect.pos + rt->anchorMin * parentRect.size;
	dragAnchorMaxPx = parentRect.pos + rt->anchorMax * parentRect.size;

	EditorManager::getInstance().BeginEdit({ obj });
	return true;
}

void GUIGizmos::ApplyDrag() {
	if (!draggedRect) return;

	const glm::vec2 d = (mouseScreen - dragStartMouse) / dragScale;   // canvas units
	glm::vec2 offMin = dragStartOffsetMin;
	glm::vec2 offMax = dragStartOffsetMax;

	if (activeHandle == RectHandle::Move) {
		offMin += d;
		offMax += d;
	}
	else {
		bool l, r, t, b;
		EdgeFlags(activeHandle, l, r, t, b);

		const glm::vec2 minSz = glm::max(draggedRect->minSize, glm::vec2(1.0f));
		glm::vec2 mn = dragStartMin;
		glm::vec2 mx = dragStartMax;

		if (l) { mn.x = std::min(mn.x + d.x, mx.x - minSz.x); offMin.x = mn.x - dragAnchorMinPx.x; }
		if (r) { mx.x = std::max(mx.x + d.x, mn.x + minSz.x); offMax.x = mx.x - dragAnchorMaxPx.x; }
		if (t) { mn.y = std::min(mn.y + d.y, mx.y - minSz.y); offMin.y = mn.y - dragAnchorMinPx.y; }
		if (b) { mx.y = std::max(mx.y + d.y, mn.y + minSz.y); offMax.y = mx.y - dragAnchorMaxPx.y; }
	}

	draggedRect->SetOffsets(offMin, offMax);   
}

void GUIGizmos::EndDrag() {
	if (isDragging && draggedObject) {
		EditorManager::getInstance().EndEdit({ draggedObject });
	}
	isDragging = false;
	activeHandle = RectHandle::None;
	draggedObject = nullptr;
	draggedRect = nullptr;
	MouseInteractComponent::PressArbitrated = false;
}

void GUIGizmos::OnMouseButton(int button, int action, int mods) {
	if (button != GLFW_MOUSE_BUTTON_LEFT) return;

	if (action == GLFW_RELEASE) {
		EndDrag();
		return;
	}
	if (action != GLFW_PRESS) return;

	if (!ViewValid() || !EditingAllowed() || !InsideView(mouseScreen)) return;

	EditorManager& em = EditorManager::getInstance();
	Object* sel = em.selectedObject;

	if (IsUIElement(sel)) {
		RectTransformComponent* rt = sel->GetComponent<RectTransformComponent>();
		glm::vec2 mn, mx;
		float scale;
		if (GetScreenRect(rt, mn, mx, scale)) {
			RectHandle h = HitTestHandles(mn, mx, mouseScreen);
			if (h != RectHandle::None && BeginDrag(sel, h)) {
				MouseInteractComponent::PressArbitrated = true;
				return;
			}
		}
	}

	if (Object* picked = PickElementAt(mouseScreen)) {
		if (picked != sel) em.SetSelectedObject(picked);
		if (BeginDrag(picked, RectHandle::Move)) {
			MouseInteractComponent::PressArbitrated = true;   
		}
		return;
	}

	if (IsUIElement(sel)) {
		RectTransformComponent* rt = sel->GetComponent<RectTransformComponent>();
		glm::vec2 mn, mx;
		float scale;
		if (GetScreenRect(rt, mn, mx, scale) && PointInRect(mouseScreen, mn, mx)) {
			if (BeginDrag(sel, RectHandle::Move)) MouseInteractComponent::PressArbitrated = true;
			return;
		}

		em.SetSelectedObject(nullptr);
	}
}

void GUIGizmos::OnCursorPosition(double xpos, double ypos) {
	mouseScreen = glm::vec2((float)xpos, (float)ypos);

	if (!isDragging) return;

	if (!InputManager::mouseLeftHold || EditorManager::getInstance().selectedObject != draggedObject) {
		EndDrag();
		return;
	}
	ApplyDrag();
}