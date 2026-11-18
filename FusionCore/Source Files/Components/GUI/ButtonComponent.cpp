#include "../../../Header Files/Components/GUI/ButtonComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cfloat>
#include <cstring>

namespace {
	EditorField::FileSpec MakeFontSpec() {
		EditorField::FileSpec s;
		s.dialog.title = "Choose Font";
		s.dialog.filters = { { "Fonts", "*.ttf;*.otf" }, { "All Files", "*.*" } };
		s.extensions = { ".ttf", ".otf" };
		s.hint = "Default (click to choose...)";
		s.storeVirtual = true;
		s.displayVirtual = true;
		s.allowClear = true;
		return s;
	}

	bool UIInputEnabled() {
		return EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate;
	}

	ImU32 ToU32(const glm::vec4& c, float alphaMul = 1.0f) {
		return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a * alphaMul));
	}
}

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

	if (!interactable || !UIInputEnabled()) {
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

void ButtonComponent::DrawUI(const UIDrawContext& c) {
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	if (w <= 0.0f || h <= 0.0f) return;

	UpdateInput(mn, mx);

	const float radius = std::min(std::max(cornerRadius, 0.0f) * c.scale, std::min(w, h) * 0.5f);

	const glm::vec4& bg =
		!interactable ? disabledColor :
		(pressed && hovered) ? pressedColor :
		hovered ? hoverColor : normalColor;

	if (bg.a > 0.0f)
		c.dl->AddRectFilled(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), ToU32(bg), radius);

	if (borderThickness > 0.0f && borderColor.a > 0.0f) {
		const float t = borderThickness * c.scale;
		const float inset = t * 0.5f;
		if (w > t && h > t) {
			c.dl->AddRect(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset),
				ToU32(borderColor), std::max(radius - inset, 0.0f), 0, t);
		}
	}

	if (!text.empty() && fontSize > 0.0f && textColor.a > 0.0f) {
		ImFont* font = fontPath.empty() ? nullptr : GUIEngine::getInstance().GetFont(fontPath);
		if (!font) font = ImGui::GetFont();

		const float px = fontSize * c.scale;
		const ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text.c_str());
		const ImVec2 pos(mn.x + (w - ts.x) * 0.5f, mn.y + (h - ts.y) * 0.5f);

		c.dl->PushClipRect(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), true);
		c.dl->AddText(font, px, pos, ToU32(textColor, interactable ? 1.0f : 0.5f), text.c_str());
		c.dl->PopClipRect();
	}
}

void ButtonComponent::ProcessInspectorUI() {
	static const EditorField::FileSpec fontSpec = MakeFontSpec();

	char buf[256];
	strncpy_s(buf, sizeof(buf), text.c_str(), _TRUNCATE);
	EditorField::InputTextScene(parent, "Text ", "##BtnText", buf, sizeof(buf), [&] { text = buf; });

	EditorField::FileScene(parent, "Font ", "##BtnFont", fontPath, [&] {}, fontSpec);
	if (!fontPath.empty() && !GUIEngine::getInstance().GetFont(fontPath))
		ImGui::TextDisabled("Could not load font, using default.");

	float fs = fontSize;
	EditorField::InputFloatScene(parent, "Font Size ", "##BtnFontSize", &fs, [&] {
		fontSize = std::max(fs, 1.0f);
		}, "%.1f");

	float tcol[4] = { textColor.r, textColor.g, textColor.b, textColor.a };
	EditorField::ColorEdit4Scene(parent, "Text Color ", "##BtnTextColor", tcol, [&] {
		textColor = glm::vec4(tcol[0], tcol[1], tcol[2], tcol[3]);
		});

	ImGui::Separator();

	EditorField::CheckboxScene(parent, "Interactable ", "##BtnInteractable", &interactable, [&] {});

	float ncol[4] = { normalColor.r, normalColor.g, normalColor.b, normalColor.a };
	EditorField::ColorEdit4Scene(parent, "Normal ", "##BtnNormal", ncol, [&] {
		normalColor = glm::vec4(ncol[0], ncol[1], ncol[2], ncol[3]);
		});

	float hcol[4] = { hoverColor.r, hoverColor.g, hoverColor.b, hoverColor.a };
	EditorField::ColorEdit4Scene(parent, "Hover ", "##BtnHover", hcol, [&] {
		hoverColor = glm::vec4(hcol[0], hcol[1], hcol[2], hcol[3]);
		});

	float pcol[4] = { pressedColor.r, pressedColor.g, pressedColor.b, pressedColor.a };
	EditorField::ColorEdit4Scene(parent, "Pressed ", "##BtnPressed", pcol, [&] {
		pressedColor = glm::vec4(pcol[0], pcol[1], pcol[2], pcol[3]);
		});

	float dcol[4] = { disabledColor.r, disabledColor.g, disabledColor.b, disabledColor.a };
	EditorField::ColorEdit4Scene(parent, "Disabled ", "##BtnDisabled", dcol, [&] {
		disabledColor = glm::vec4(dcol[0], dcol[1], dcol[2], dcol[3]);
		});

	ImGui::Separator();

	float cr = cornerRadius;
	EditorField::InputFloatScene(parent, "Corner Radius ", "##BtnRadius", &cr, [&] {
		cornerRadius = std::max(cr, 0.0f);
		}, "%.1f");

	float bt = borderThickness;
	EditorField::InputFloatScene(parent, "Border Thickness ", "##BtnBorderThickness", &bt, [&] {
		borderThickness = std::max(bt, 0.0f);
		}, "%.1f");

	float bcol[4] = { borderColor.r, borderColor.g, borderColor.b, borderColor.a };
	EditorField::ColorEdit4Scene(parent, "Border Color ", "##BtnBorderColor", bcol, [&] {
		borderColor = glm::vec4(bcol[0], bcol[1], bcol[2], bcol[3]);
		});

	ImGui::TextDisabled("Buttons react to the mouse while the game is running.");
}

void ButtonComponent::CopyTo(Object* other) {
	ButtonComponent* target = other->GetComponent<ButtonComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<ButtonComponent>(other));
		target = other->GetComponent<ButtonComponent>();
	}
	target->SetEnabled(Enabled);
	target->text = text;
	target->fontPath = fontPath;
	target->fontSize = fontSize;
	target->textColor = textColor;
	target->normalColor = normalColor;
	target->hoverColor = hoverColor;
	target->pressedColor = pressedColor;
	target->disabledColor = disabledColor;
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
}