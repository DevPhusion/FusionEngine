#include "../../../Header Files/Components/GUI/PanelComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>
#include <algorithm>

PanelComponent::PanelComponent(Object* parent) : ComponentBase<PanelComponent>(parent) {
	Name = "Panel Component";
}

void PanelComponent::Activate() {
	Component::Activate();

	if (!parent->GetComponent<RectTransformComponent>())
		parent->AddComponent(std::make_unique<RectTransformComponent>(parent));

	if (Enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
}

void PanelComponent::Deactivate() {
	Component::Deactivate();
	GUIEngine::getInstance().UnregisterDrawable(parent, this);
}

void PanelComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (!isActive) return;
	if (enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
	else         GUIEngine::getInstance().UnregisterDrawable(parent, this);
}

void PanelComponent::OnDelete() {
	Deactivate();
}

void PanelComponent::DrawUI(const UIDrawContext& c) {
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	if (w <= 0.0f || h <= 0.0f) return;

	const float radius = std::min(std::max(cornerRadius, 0.0f) * c.scale, std::min(w, h) * 0.5f);

	if (color.a > 0.0f) {
		const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
		c.dl->AddRectFilled(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), col, radius);
	}

	if (borderThickness > 0.0f && borderColor.a > 0.0f) {
		const float t = borderThickness * c.scale;
		const float inset = t * 0.5f;
		if (w > t && h > t) {
			const ImU32 bcol = ImGui::ColorConvertFloat4ToU32(
				ImVec4(borderColor.r, borderColor.g, borderColor.b, borderColor.a));
			c.dl->AddRect(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset),
				bcol, std::max(radius - inset, 0.0f), 0, t);
		}
	}
}

void PanelComponent::ProcessInspectorUI() {
	float col[4] = { color.r, color.g, color.b, color.a };
	EditorField::ColorEdit4Scene(parent, "Color ", "##PanelColor", col, [&] {
		color = glm::vec4(col[0], col[1], col[2], col[3]);
		});

	float cr = cornerRadius;
	EditorField::InputFloatScene(parent, "Corner Radius ", "##PanelRadius", &cr, [&] {
		cornerRadius = std::max(cr, 0.0f);
		}, "%.1f");

	ImGui::Separator();

	float bt = borderThickness;
	EditorField::InputFloatScene(parent, "Border Thickness ", "##PanelBorderThickness", &bt, [&] {
		borderThickness = std::max(bt, 0.0f);
		}, "%.1f");

	float bcol[4] = { borderColor.r, borderColor.g, borderColor.b, borderColor.a };
	EditorField::ColorEdit4Scene(parent, "Border Color ", "##PanelBorderColor", bcol, [&] {
		borderColor = glm::vec4(bcol[0], bcol[1], bcol[2], bcol[3]);
		});
}

void PanelComponent::CopyTo(Object* other) {
	PanelComponent* target = other->GetComponent<PanelComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<PanelComponent>(other));
		target = other->GetComponent<PanelComponent>();
	}
	target->SetEnabled(Enabled);
	target->color = color;
	target->cornerRadius = cornerRadius;
	target->borderColor = borderColor;
	target->borderThickness = borderThickness;
}

void PanelComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(color);
	w.Write(cornerRadius);
	w.Write(borderColor);
	w.Write(borderThickness);
}

void PanelComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	color = r.Read<glm::vec4>();
	cornerRadius = r.Read<float>();
	borderColor = r.Read<glm::vec4>();
	borderThickness = r.Read<float>();
}