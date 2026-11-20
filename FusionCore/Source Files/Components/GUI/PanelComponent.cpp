#include "../../../Header Files/Components/GUI/PanelComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>

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
	texture.Release();
}

void PanelComponent::DrawUI(const UIDrawContext& c) {
	const float radius = GUIEngine::ScaledRadius(c, cornerRadius);

	if (mode == PanelMode::Texture)
		GUIEngine::DrawTexture(c, texture.Resolve(texturePath), tint, radius);
	else
		GUIEngine::DrawFill(c, color, radius);

	GUIEngine::DrawBorder(c, borderColor, borderThickness, radius);
}

void PanelComponent::ProcessInspectorUI() {
	static const char* modeNames[] = { "Color", "Texture" };
	static const EditorField::FileSpec textureSpec = EditorField::FileSpec::ImageFile(true, 96.0f);

	int pm = (int)mode;
	EditorField::ComboScene(parent, "Mode ", "##PanelMode", &pm, modeNames, 2, [&] {
		mode = (PanelMode)pm;
		});

	if (mode == PanelMode::Color) {
		EditorField::ColorEdit4Scene(parent, "Color ", "##PanelColor", &color, [&] {});
	}
	else {
		EditorField::FileScene(parent, "Texture ", "##PanelTexture", texturePath, [&] {}, textureSpec);

		if (texturePath.empty())
			ImGui::TextDisabled("No texture selected.");
		else if (texture.Resolve(texturePath) == 0)
			ImGui::TextDisabled("Could not load texture.");

		EditorField::ColorEdit4Scene(parent, "Tint ", "##PanelTint", &tint, [&] {});
	}

	EditorField::InputFloatScene(parent, "Corner Radius ", "##PanelRadius", &cornerRadius, [&] {});

	ImGui::Separator();

	EditorField::InputFloatScene(parent, "Border Thickness ", "##PanelBorderThickness", &borderThickness, [&] {});
	EditorField::ColorEdit4Scene(parent, "Border Color ", "##PanelBorderColor", &borderColor, [&] {});
}

void PanelComponent::CopyTo(Object* other) {
	PanelComponent* target = other->GetComponent<PanelComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<PanelComponent>(other));
		target = other->GetComponent<PanelComponent>();
	}
	target->SetEnabled(Enabled);
	target->mode = mode;
	target->color = color;
	target->texturePath = texturePath;   
	target->tint = tint;
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
	w.Write((int)mode);
	w.WriteString(texturePath);
	w.Write(tint);
}

void PanelComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	color = r.Read<glm::vec4>();
	cornerRadius = r.Read<float>();
	borderColor = r.Read<glm::vec4>();
	borderThickness = r.Read<float>();
	mode = (PanelMode)r.Read<int>();
	texturePath = r.ReadString();   
	tint = r.Read<glm::vec4>();
}