#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Components/GUI/CanvasComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cstdio>

RectTransformComponent::RectTransformComponent(Object* parent) : ComponentBase<RectTransformComponent>(parent) {
	Name = "Rect Transform Component";
	CanDisable = false;
}

void RectTransformComponent::Activate() {
	Component::Activate();
	MarkDirty();
}

void RectTransformComponent::OnDelete() {
}

CanvasComponent* RectTransformComponent::FindCanvas() const {
	if (!parent) return nullptr;
	for (Object* o = parent->parent; o; o = o->parent) {
		if (CanvasComponent* c = o->GetComponent<CanvasComponent>()) return c;
	}
	return nullptr;
}

UiRect RectTransformComponent::GetParentRect() const {
	if (!parent || !parent->parent) return UiRect{};
	Object* p = parent->parent;

	if (CanvasComponent* canvas = p->GetComponent<CanvasComponent>())
		return UiRect{ glm::vec2(0.0f), canvas->GetCanvasSize() };
	if (RectTransformComponent* prt = p->GetComponent<RectTransformComponent>())
		return prt->rect;
	return UiRect{};
}

void RectTransformComponent::MarkDirty() {
	if (CanvasComponent* canvas = FindCanvas()) canvas->MarkLayoutDirty();
}

void RectTransformComponent::Resolve(const UiRect& parentRect) {
	const glm::vec2 anchorMinPx = parentRect.pos + anchorMin * parentRect.size;
	const glm::vec2 anchorMaxPx = parentRect.pos + anchorMax * parentRect.size;

	const glm::vec2 mn = anchorMinPx + offsetMin;
	const glm::vec2 mx = anchorMaxPx + offsetMax;

	rect.pos = mn;
	rect.size = glm::max(mx - mn, minSize);
}

void RectTransformComponent::ResolveTree(const UiRect& parentRect) {
	Resolve(parentRect);
	for (Object* child : parent->children) {
		if (RectTransformComponent* crt = child->GetComponent<RectTransformComponent>())
			crt->ResolveTree(rect);
	}
}

void RectTransformComponent::SetAnchors(glm::vec2 mn, glm::vec2 mx) {
	if (mn == anchorMin && mx == anchorMax) return;
	anchorMin = mn;
	anchorMax = mx;
	MarkDirty();
}

void RectTransformComponent::SetOffsets(glm::vec2 mn, glm::vec2 mx) {
	if (mn == offsetMin && mx == offsetMax) return;
	offsetMin = mn;
	offsetMax = mx;
	MarkDirty();
}

void RectTransformComponent::SetMinSize(glm::vec2 ms) {
	ms = glm::max(ms, glm::vec2(0.0f));
	if (ms == minSize) return;
	minSize = ms;
	MarkDirty();
}

void RectTransformComponent::SetAnchoredRect(glm::vec2 center, glm::vec2 size) {
	SetOffsets(center - size * 0.5f, center + size * 0.5f);
}

void RectTransformComponent::ProcessInspectorUI() {
	if (!IsStretched()) {
		const glm::vec2 size = offsetMax - offsetMin;
		const glm::vec2 center = (offsetMin + offsetMax) * 0.5f;

		float pos[2] = { center.x, center.y };
		EditorField::InputFloat2Scene(parent, "Position ", "##RtPos", pos, [&] {
			SetAnchoredRect(glm::vec2(pos[0], pos[1]), size);
			});

		float sz[2] = { size.x, size.y };
		EditorField::InputFloat2Scene(parent, "Size ", "##RtSize", sz, [&] {
			SetAnchoredRect(center, glm::max(glm::vec2(sz[0], sz[1]), glm::vec2(0.0f)));
			});
	}
	else {
		float mn[2] = { offsetMin.x, offsetMin.y };
		EditorField::InputFloat2Scene(parent, "Offset Min ", "##RtOffMin", mn, [&] {
			SetOffsets(glm::vec2(mn[0], mn[1]), offsetMax);
			});

		float mx[2] = { offsetMax.x, offsetMax.y };
		EditorField::InputFloat2Scene(parent, "Offset Max ", "##RtOffMax", mx, [&] {
			SetOffsets(offsetMin, glm::vec2(mx[0], mx[1]));
			});
	}

	ImGui::Separator();

	float amin[2] = { anchorMin.x, anchorMin.y };
	EditorField::InputFloat2Scene(parent, "Anchor Min ", "##RtAnchorMin", amin, [&] {
		SetAnchors(glm::vec2(amin[0], amin[1]), anchorMax);
		}, "%.2f");

	float amax[2] = { anchorMax.x, anchorMax.y };
	EditorField::InputFloat2Scene(parent, "Anchor Max ", "##RtAnchorMax", amax, [&] {
		SetAnchors(anchorMin, glm::vec2(amax[0], amax[1]));
		}, "%.2f");

	float ms[2] = { minSize.x, minSize.y };
	EditorField::InputFloat2Scene(parent, "Min Size ", "##RtMinSize", ms, [&] {
		SetMinSize(glm::vec2(ms[0], ms[1]));
		});

	ImGui::Separator();

	if (EditorField::BeginPropertyTable("##RtInfo")) {
		char buf[96];
		std::snprintf(buf, sizeof(buf), "%.0f, %.0f   %.0f x %.0f", rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
		EditorField::PropertyRow("Rect", buf);
		EditorField::EndPropertyTable();
	}
}

void RectTransformComponent::CopyTo(Object* other) {
	RectTransformComponent* target = other->GetComponent<RectTransformComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<RectTransformComponent>(other));
		target = other->GetComponent<RectTransformComponent>();
	}
	target->SetEnabled(Enabled);
	target->anchorMin = anchorMin;
	target->anchorMax = anchorMax;
	target->offsetMin = offsetMin;
	target->offsetMax = offsetMax;
	target->minSize = minSize;
	target->rect = rect;
	target->MarkDirty();
}

void RectTransformComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(anchorMin);
	w.Write(anchorMax);
	w.Write(offsetMin);
	w.Write(offsetMax);
	w.Write(minSize);
}

void RectTransformComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	anchorMin = r.Read<glm::vec2>();
	anchorMax = r.Read<glm::vec2>();
	offsetMin = r.Read<glm::vec2>();
	offsetMax = r.Read<glm::vec2>();
	minSize = r.Read<glm::vec2>();
}