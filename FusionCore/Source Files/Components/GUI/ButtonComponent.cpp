#include "../../../Header Files/Components/GUI/ButtonComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>

ButtonComponent::ButtonComponent(Object* parent) : ComponentBase<ButtonComponent>(parent) {
	Name = "Button Component";
}

void ButtonComponent::Activate() {
	Component::Activate();

	if (!parent->GetComponent<RectTransformComponent>())
		parent->AddComponent(std::make_unique<RectTransformComponent>(parent));

	if (Enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
}

void ButtonComponent::Deactivate() {
	Component::Deactivate();
	GUIEngine::getInstance().UnregisterDrawable(parent, this);
	hovered = false;
	pressed = false;
}

void ButtonComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (!isActive) return;
	if (enabled) {
		GUIEngine::getInstance().RegisterDrawable(parent, this);
	}
	else {
		GUIEngine::getInstance().UnregisterDrawable(parent, this);
		hovered = false;
		pressed = false;
	}
}

void ButtonComponent::OnDelete() {
	Deactivate();
	ClearCallbacks();
	for (GUITextureHandle& t : textures) t.Release();
}

int ButtonComponent::AddCallback(ButtonEvent event, Callback cb) {
	if (!cb) return -1;
	const int id = nextCallbackId++;
	callbacks[(int)event][id] = std::move(cb);
	return id;
}

void ButtonComponent::RemoveCallback(int id) {
	for (auto& m : callbacks) m.erase(id);
}

void ButtonComponent::ClearCallbacks() {
	for (auto& m : callbacks) m.clear();
}

void ButtonComponent::Fire(ButtonEvent event) {
	auto& live = callbacks[(int)event];
	if (live.empty()) return;

	const auto snapshot = live;
	for (const auto& [id, fn] : snapshot) {
		if (live.find(id) == live.end()) continue;  
		fn();
	}
}

void ButtonComponent::Click() {
	if (!interactable) return;
	Fire(ButtonEvent::Click);
}

void ButtonComponent::UpdateInput(const glm::vec2& mn, const glm::vec2& mx) {
	const int frame = ImGui::GetFrameCount();
	if (frame == lastInputFrame) return;
	lastInputFrame = frame;

	if (!interactable || !GUIEngine::InputEnabled()) {
		hovered = false;
		pressed = false;
		return;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const bool inside = ImGui::IsMousePosValid(&io.MousePos) &&
		io.MousePos.x >= mn.x && io.MousePos.x < mx.x &&
		io.MousePos.y >= mn.y && io.MousePos.y < mx.y;

	if (inside != hovered) {
		hovered = inside;
		Fire(inside ? ButtonEvent::HoverEnter : ButtonEvent::HoverExit);
	}

	if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		pressed = true;
		Fire(ButtonEvent::Press);
	}

	if (pressed && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		pressed = false;
		Fire(ButtonEvent::Release);
		if (inside) Fire(ButtonEvent::Click);
	}

	if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

const std::string& ButtonComponent::TexturePathFor(int state) const {
	switch (state) {
	case Hover:    return hoverTexturePath;
	case Pressed:  return pressedTexturePath;
	case Disabled: return disabledTexturePath;
	default:       return texturePath;
	}
}

const glm::vec4& ButtonComponent::TintFor(int state) const {
	switch (state) {
	case Hover:    return hoverTint;
	case Pressed:  return pressedTint;
	case Disabled: return disabledTint;
	default:       return normalTint;
	}
}

void ButtonComponent::DrawUI(const UIDrawContext& c) {
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	if (mx.x - mn.x <= 0.0f || mx.y - mn.y <= 0.0f) return;

	UpdateInput(mn, mx);

	const int state = !interactable ? Disabled : (pressed && hovered) ? Pressed : hovered ? Hover : Normal;
	const float radius = GUIEngine::ScaledRadius(c, cornerRadius);

	if (mode == ButtonMode::Texture) {
		unsigned int tex[StateCount];
		for (int i = 0; i < StateCount; ++i) tex[i] = textures[i].Resolve(TexturePathFor(i));

		const unsigned int use = tex[state] != 0 ? tex[state] : tex[Normal];   
		GUIEngine::DrawTexture(c, use, TintFor(state), radius);
	}
	else {
		const glm::vec4& bg =
			state == Disabled ? disabledColor :
			state == Pressed ? pressedColor :
			state == Hover ? hoverColor : normalColor;
		GUIEngine::DrawFill(c, bg, radius);
	}

	GUIEngine::DrawBorder(c, borderColor, borderThickness, radius);

	const glm::vec4 labelColor(textColor.r, textColor.g, textColor.b, textColor.a * (interactable ? 1.0f : 0.5f));
	GUIEngine::DrawCenteredText(c, text, GUIEngine::getInstance().GetFontOrDefault(fontPath), fontSize, labelColor);
}

void ButtonComponent::ProcessInspectorUI() {
	static const char* modeNames[] = { "Color", "Texture" };
	static const EditorField::FileSpec fontSpec = EditorField::FileSpec::FontFile();
	static const EditorField::FileSpec textureSpec = EditorField::FileSpec::ImageFile(true, 96.0f);
	static const EditorField::FileSpec stateSpec = [] {
		EditorField::FileSpec s = EditorField::FileSpec::ImageFile(true);
		s.preview = EditorField::FileSpec::Preview::None;   
		return s;
		}();

	char buf[4096];
	strncpy_s(buf, sizeof(buf), text.c_str(), _TRUNCATE);
	EditorField::Wrap(parent, "Text ",
		[&] {
			return ImGui::InputTextMultiline("##TextStr", buf, sizeof(buf), ImVec2(-FLT_MIN, 90.0f));
		},
		[&] { text = buf; });

	EditorField::FileScene(parent, "Font ", "##BtnFont", fontPath, [&] {}, fontSpec);
	if (!fontPath.empty() && !GUIEngine::getInstance().GetFont(fontPath))
		ImGui::TextDisabled("Could not load font, using default.");

	EditorField::InputFloatScene(parent, "Font Size ", "##BtnFontSize", &fontSize, [&]
		{fontSize = std::max(fontSize, 1.0f);});
	EditorField::ColorEdit4Scene(parent, "Text Color ", "##BtnTextColor", &textColor, [&] {});

	ImGui::Separator();

	EditorField::CheckboxScene(parent, "Interactable ", "##BtnInteractable", &interactable, [&] {});

	int bm = (int)mode;
	EditorField::ComboScene(parent, "Mode ", "##BtnMode", &bm, modeNames, 2, [&] {
		mode = (ButtonMode)bm;
		});

	if (mode == ButtonMode::Color) {
		EditorField::ColorEdit4Scene(parent, "Normal ", "##BtnNormal", &normalColor, [&] {});
		EditorField::ColorEdit4Scene(parent, "Hover ", "##BtnHover", &hoverColor, [&] {});
		EditorField::ColorEdit4Scene(parent, "Pressed ", "##BtnPressed", &pressedColor, [&] {});
		EditorField::ColorEdit4Scene(parent, "Disabled ", "##BtnDisabled", &disabledColor, [&] {});
	}
	else {
		EditorField::FileScene(parent, "Texture ", "##BtnTexture", texturePath, [&] {}, textureSpec);

		if (texturePath.empty())
			ImGui::TextDisabled("No texture selected.");
		else if (textures[Normal].Resolve(texturePath) == 0)
			ImGui::TextDisabled("Could not load texture.");

		if (ImGui::TreeNode("State Textures##BtnStateTextures")) {
			EditorField::FileScene(parent, "Hover ", "##BtnTexHover", hoverTexturePath, [&] {}, stateSpec);
			EditorField::FileScene(parent, "Pressed ", "##BtnTexPressed", pressedTexturePath, [&] {}, stateSpec);
			EditorField::FileScene(parent, "Disabled ", "##BtnTexDisabled", disabledTexturePath, [&] {}, stateSpec);
			ImGui::TextDisabled("Empty slots use the main texture.");
			ImGui::TreePop();
		}

		EditorField::ColorEdit4Scene(parent, "Normal Tint ", "##BtnTintNormal", &normalTint, [&] {});
		EditorField::ColorEdit4Scene(parent, "Hover Tint ", "##BtnTintHover", &hoverTint, [&] {});
		EditorField::ColorEdit4Scene(parent, "Pressed Tint ", "##BtnTintPressed", &pressedTint, [&] {});
		EditorField::ColorEdit4Scene(parent, "Disabled Tint ", "##BtnTintDisabled", &disabledTint, [&] {});
	}

	ImGui::Separator();

	EditorField::InputFloatScene(parent, "Corner Radius ", "##BtnRadius", &cornerRadius, [&] {});
	EditorField::InputFloatScene(parent, "Border Thickness ", "##BtnBorderThickness", &borderThickness, [&] {});
	EditorField::ColorEdit4Scene(parent, "Border Color ", "##BtnBorderColor", &borderColor, [&] {});
}

void ButtonComponent::CopyTo(Object* other) {
	ButtonComponent* target = other->GetComponent<ButtonComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<ButtonComponent>(other));
		target = other->GetComponent<ButtonComponent>();
	}
	target->SetEnabled(Enabled);
	target->mode = mode;
	target->text = text;
	target->fontPath = fontPath;
	target->fontSize = fontSize;
	target->textColor = textColor;
	target->normalColor = normalColor;
	target->hoverColor = hoverColor;
	target->pressedColor = pressedColor;
	target->disabledColor = disabledColor;
	target->texturePath = texturePath;           
	target->hoverTexturePath = hoverTexturePath;
	target->pressedTexturePath = pressedTexturePath;
	target->disabledTexturePath = disabledTexturePath;
	target->normalTint = normalTint;
	target->hoverTint = hoverTint;
	target->pressedTint = pressedTint;
	target->disabledTint = disabledTint;
	target->cornerRadius = cornerRadius;
	target->borderColor = borderColor;
	target->borderThickness = borderThickness;
	target->interactable = interactable;
}

void ButtonComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.WriteString(text);
	w.WriteString(fontPath);
	w.Write(fontSize);
	w.Write(textColor);
	w.Write(normalColor);
	w.Write(hoverColor);
	w.Write(pressedColor);
	w.Write(disabledColor);
	w.Write(cornerRadius);
	w.Write(borderColor);
	w.Write(borderThickness);
	w.Write(interactable);
	w.Write((int)mode);
	w.WriteString(texturePath);
	w.WriteString(hoverTexturePath);
	w.WriteString(pressedTexturePath);
	w.WriteString(disabledTexturePath);
	w.Write(normalTint);
	w.Write(hoverTint);
	w.Write(pressedTint);
	w.Write(disabledTint);
}

void ButtonComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	text = r.ReadString();
	fontPath = r.ReadString();
	fontSize = r.Read<float>();
	textColor = r.Read<glm::vec4>();
	normalColor = r.Read<glm::vec4>();
	hoverColor = r.Read<glm::vec4>();
	pressedColor = r.Read<glm::vec4>();
	disabledColor = r.Read<glm::vec4>();
	cornerRadius = r.Read<float>();
	borderColor = r.Read<glm::vec4>();
	borderThickness = r.Read<float>();
	interactable = r.Read<bool>();
	mode = (ButtonMode)r.Read<int>();
	texturePath = r.ReadString();   
	hoverTexturePath = r.ReadString();
	pressedTexturePath = r.ReadString();
	disabledTexturePath = r.ReadString();
	normalTint = r.Read<glm::vec4>();
	hoverTint = r.Read<glm::vec4>();
	pressedTint = r.Read<glm::vec4>();
	disabledTint = r.Read<glm::vec4>();
}