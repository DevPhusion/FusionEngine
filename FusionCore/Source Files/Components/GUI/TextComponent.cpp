#include "../../../Header Files/Components/GUI/TextComponent.h"
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
}

TextComponent::TextComponent(Object* parent) : ComponentBase<TextComponent>(parent) {
	Name = "Text Component";
}

void TextComponent::Activate() {
	Component::Activate();

	if (!parent->GetComponent<RectTransformComponent>())
		parent->AddComponent(std::make_unique<RectTransformComponent>(parent));

	if (Enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
}

void TextComponent::Deactivate() {
	Component::Deactivate();
	GUIEngine::getInstance().UnregisterDrawable(parent, this);
}

void TextComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (!isActive) return;
	if (enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
	else         GUIEngine::getInstance().UnregisterDrawable(parent, this);
}

void TextComponent::OnDelete() {
	Deactivate();
}

void TextComponent::DrawUI(const UIDrawContext& c) {
	if (text.empty() || color.a <= 0.0f || fontSize <= 0.0f) return;

	ImFont* font = fontPath.empty() ? nullptr : GUIEngine::getInstance().GetFont(fontPath);
	if (!font) font = ImGui::GetFont();  

	const float px = fontSize * c.scale;

	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	if (w <= 0.0f || h <= 0.0f) return;

	const float wrap = wordWrap ? w : 0.0f;
	const ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, wrap, text.c_str());

	float x = mn.x;
	if (hAlign == HAlign::Center)     x += (w - ts.x) * 0.5f;
	else if (hAlign == HAlign::Right) x += (w - ts.x);

	float y = mn.y;
	if (vAlign == VAlign::Middle)      y += (h - ts.y) * 0.5f;
	else if (vAlign == VAlign::Bottom) y += (h - ts.y);

	const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));

	c.dl->PushClipRect(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), true);
	c.dl->AddText(font, px, ImVec2(x, y), col, text.c_str(), nullptr, wrap);
	c.dl->PopClipRect();
}

void TextComponent::ProcessInspectorUI() {
	static const char* hNames[] = { "Left", "Center", "Right" };
	static const char* vNames[] = { "Top", "Middle", "Bottom" };
	static const EditorField::FileSpec fontSpec = MakeFontSpec();

	char buf[4096];
	strncpy_s(buf, sizeof(buf), text.c_str(), _TRUNCATE);
	EditorField::Wrap(parent, "Text ",
		[&] { return ImGui::InputTextMultiline("##TextStr", buf, sizeof(buf), ImVec2(-FLT_MIN, 60.0f)); },
		[&] { text = buf; });

	EditorField::FileScene(parent, "Font ", "##TextFont", fontPath, [&] {}, fontSpec);
	if (!fontPath.empty() && !GUIEngine::getInstance().GetFont(fontPath))
		ImGui::TextDisabled("Could not load font, using default.");

	float fs = fontSize;
	EditorField::InputFloatScene(parent, "Font Size ", "##TextSize", &fs, [&] {
		fontSize = std::max(fs, 1.0f);
		}, "%.1f");

	float col[4] = { color.r, color.g, color.b, color.a };
	EditorField::ColorEdit4Scene(parent, "Color ", "##TextColor", col, [&] {
		color = glm::vec4(col[0], col[1], col[2], col[3]);
		});

	ImGui::Separator();

	int ha = (int)hAlign;
	EditorField::ComboScene(parent, "H Align ", "##TextHAlign", &ha, hNames, 3, [&] {
		hAlign = (HAlign)ha;
		});

	int va = (int)vAlign;
	EditorField::ComboScene(parent, "V Align ", "##TextVAlign", &va, vNames, 3, [&] {
		vAlign = (VAlign)va;
		});

	EditorField::CheckboxScene(parent, "Word Wrap ", "##TextWrap", &wordWrap, [&] {});
}

void TextComponent::CopyTo(Object* other) {
	TextComponent* target = other->GetComponent<TextComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<TextComponent>(other));
		target = other->GetComponent<TextComponent>();
	}
	target->SetEnabled(Enabled);
	target->text = text;
	target->fontPath = fontPath;
	target->color = color;
	target->fontSize = fontSize;
	target->hAlign = hAlign;
	target->vAlign = vAlign;
	target->wordWrap = wordWrap;
}

void TextComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.WriteString(text);
	w.Write(color);
	w.Write(fontSize);
	w.Write((int)hAlign);
	w.Write((int)vAlign);
	w.Write(wordWrap);
	w.WriteString(fontPath);   
}

void TextComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	text = r.ReadString();
	color = r.Read<glm::vec4>();
	fontSize = r.Read<float>();
	hAlign = (HAlign)r.Read<int>();
	vAlign = (VAlign)r.Read<int>();
	wordWrap = r.Read<bool>();
	fontPath = r.ReadString();
}