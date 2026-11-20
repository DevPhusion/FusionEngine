#include "../../../Header Files/Components/GUI/TextComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

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

void TextComponent::ParseRich() {
	richCache = RichText::Parse(text, IsRich());
	richCacheSrc = text;
	richCacheRich = IsRich();
	richCacheValid = true;
}

void TextComponent::DrawUI(const UIDrawContext& c) {
	if (text.empty() || color.a <= 0.0f || fontSize <= 0.0f) return;

	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	if (w <= 0.0f || h <= 0.0f) return;

	if (!richCacheValid || richCacheRich != IsRich() || richCacheSrc != text) ParseRich();

	// ---- resolve the four font slots: index = bold | (italic << 1) ----
	struct FontSel { ImFont* font; bool fakeBold; bool fakeItalic; };
	GUIEngine& gui = GUIEngine::getInstance();

	ImFont* reg = gui.GetFontOrDefault(fontPath);
	ImFont* bf = gui.GetFont(boldFontPath);
	ImFont* itf = gui.GetFont(italicFontPath);
	ImFont* bif = gui.GetFont(boldItalicFontPath);

	FontSel fonts[4];
	fonts[0] = { reg, false, false };
	fonts[1] = { bf ? bf : reg, bf == nullptr, false };
	fonts[2] = { itf ? itf : reg, false, itf == nullptr };
	if (bif)      fonts[3] = { bif, false, false };
	else if (bf)  fonts[3] = { bf, false, true };
	else if (itf) fonts[3] = { itf, true, false };
	else          fonts[3] = { reg, true, true };

	auto boldOffset = [](float px) { return std::max(0.5f, px * 0.035f); };
	auto fontIndex = [](const RichText::Piece& p) { return (p.bold ? 1 : 0) | (p.italic ? 2 : 0); };
	auto measure = [&](const char* b, const char* e, float px, int fi) {
		float mw = fonts[fi].font->CalcTextSizeA(px, FLT_MAX, 0.0f, b, e).x;
		if (fonts[fi].fakeBold) mw += boldOffset(px);
		return mw;
		};

	// ---- layout ----
	struct Placed {
		const RichText::Piece* p;
		const char* b;
		const char* e;
		float x, w, px;
		int fi;
	};
	struct Line {
		std::vector<Placed> items;
		float width = 0.0f, height = 0.0f;
	};

	const float basePx = fontSize * c.scale;
	const float wrap = wordWrap ? w : 0.0f;

	std::vector<Line> lines;
	Line cur;
	bool wrapped = false;

	auto flush = [&]() {
		while (!cur.items.empty() && cur.items.back().p->space) {   // trailing spaces don't count
			cur.width -= cur.items.back().w;
			cur.items.pop_back();
		}
		if (cur.height <= 0.0f) cur.height = basePx;
		lines.push_back(std::move(cur));
		cur = Line();
		};
	auto addItem = [&](const RichText::Piece* p, const char* b, const char* e, float iw, float px, int fi) {
		cur.items.push_back({ p, b, e, cur.width, iw, px, fi });
		cur.width += iw;
		cur.height = std::max(cur.height, px);
		};

	for (const RichText::Piece& p : richCache) {
		if (p.newline) { flush(); wrapped = false; continue; }

		const float px = (p.size > 0.0f ? p.size : fontSize) * c.scale;
		const int fi = fontIndex(p);
		const char* b = p.text.c_str();
		const char* e = b + p.text.size();
		const float pw = measure(b, e, px, fi);

		if (wrap > 0.0f && !p.space && !cur.items.empty() && cur.width + pw > wrap) {
			flush();
			wrapped = true;
		}
		if (p.space && cur.items.empty() && wrapped) continue;   // no leading spaces after a soft wrap

		if (wrap > 0.0f && !p.space && pw > wrap) {
			// Single word wider than the box: break it by character.
			const char* s = b;
			while (s < e) {
				const float avail = wrap - cur.width;
				const char* q = s;
				float acc = 0.0f;
				while (q < e) {
					const char* n = RichText::NextUtf8(q, e);
					const float cw = measure(q, n, px, fi);
					if (acc + cw > avail && q > s) break;
					if (acc + cw > avail && q == s) break;
					acc += cw;
					q = n;
				}
				if (q == s) {
					if (!cur.items.empty()) { flush(); wrapped = true; continue; }
					q = RichText::NextUtf8(s, e);            // line empty: must take at least one char
					acc = measure(s, q, px, fi);
				}
				addItem(&p, s, q, acc, px, fi);
				s = q;
				if (s < e) { flush(); wrapped = true; }
			}
		}
		else {
			addItem(&p, b, e, pw, px, fi);
		}
	}
	if (!cur.items.empty() || lines.empty()) flush();

	float totalH = 0.0f;
	for (const Line& l : lines) totalH += l.height;

	// ---- draw ----
	ImDrawList* dl = c.dl;
	dl->PushClipRect(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), true);

	float y = mn.y;
	if (vAlign == VAlign::Middle)      y += (h - totalH) * 0.5f;
	else if (vAlign == VAlign::Bottom) y += (h - totalH);

	for (const Line& line : lines) {
		float x0 = mn.x;
		if (hAlign == HAlign::Center)     x0 += (w - line.width) * 0.5f;
		else if (hAlign == HAlign::Right) x0 += (w - line.width);

		for (const Placed& it : line.items) {
			const RichText::Piece& p = *it.p;
			if (p.newline) continue;

			glm::vec4 cc = color;
			if (p.hasColor) cc = glm::vec4(p.color.r, p.color.g, p.color.b, p.color.a * color.a);
			if (cc.a <= 0.0f) continue;
			const ImU32 col = GUIEngine::ToU32(cc);

			const float x = x0 + it.x;
			const float ty = y + (line.height - it.px);   // bottom-align mixed sizes on a line
			const FontSel& fs = fonts[it.fi];

			if (!p.space) {
				const int vtxStart = dl->VtxBuffer.Size;
				dl->AddText(fs.font, it.px, ImVec2(x, ty), col, it.b, it.e);
				if (fs.fakeBold)
					dl->AddText(fs.font, it.px, ImVec2(x + boldOffset(it.px), ty), col, it.b, it.e);

				if (fs.fakeItalic) {
					const float pivot = ty + it.px * 0.8f;   // approx baseline
					for (int v = vtxStart; v < dl->VtxBuffer.Size; ++v)
						dl->VtxBuffer[v].pos.x += (pivot - dl->VtxBuffer[v].pos.y) * 0.2f;
				}
			}

			const float thick = std::max(1.0f, it.px * 0.06f);
			if (p.underline)
				dl->AddLine(ImVec2(x, ty + it.px * 0.92f), ImVec2(x + it.w, ty + it.px * 0.92f), col, thick);
			if (p.strike)
				dl->AddLine(ImVec2(x, ty + it.px * 0.55f), ImVec2(x + it.w, ty + it.px * 0.55f), col, thick);
		}
		y += line.height;
	}

	dl->PopClipRect();
}

int TextComponent::SelectionCallback(ImGuiInputTextCallbackData* data) {
	TextComponent* self = static_cast<TextComponent*>(data->UserData);
	self->selStart = data->SelectionStart;
	self->selEnd = data->SelectionEnd;
	return 0;
}

void TextComponent::ProcessInspectorUI() {
	static const char* hNames[] = { "Left", "Center", "Right" };
	static const char* vNames[] = { "Top", "Middle", "Bottom" };
	static const char* modeNames[] = { "Plain", "Rich Text" };
	static const EditorField::FileSpec fontSpec = EditorField::FileSpec::FontFile();
	static const EditorField::FileSpec variantSpec = EditorField::FileSpec::FontFile("None (faux style)");

	char buf[4096];
	strncpy_s(buf, sizeof(buf), text.c_str(), _TRUNCATE);
	EditorField::Wrap(parent, "Text ",
		[&] {
			return ImGui::InputTextMultiline("##TextStr", buf, sizeof(buf), ImVec2(-FLT_MIN, 90.0f),
				ImGuiInputTextFlags_CallbackAlways, SelectionCallback, this);
		},
		[&] { text = buf; });

	int tm = (int)textMode;
	EditorField::ComboScene(parent, "Text Mode ", "##TextMode", &tm, modeNames, 2, [&] {
		textMode = (TextMode)tm;
		});

	if (IsRich()) {
		static float pick[3] = { 1.0f, 0.3f, 0.3f };
		std::string pending;
		EditorField::Wrap(parent, "Format ",
			[&] {
				bool hit = false;
				auto apply = [&](const std::string& open, const std::string& close) {
					int caret = 0;
					pending = RichText::WrapSelection(text, selStart, selEnd, open, close, &caret);
					selStart = selEnd = caret;
					hit = true;
					};

				if (ImGui::SmallButton("B")) apply("<b>", "</b>");
				ImGui::SameLine();
				if (ImGui::SmallButton("I")) apply("<i>", "</i>");
				ImGui::SameLine();
				if (ImGui::SmallButton("U")) apply("<u>", "</u>");
				ImGui::SameLine();
				if (ImGui::SmallButton("S")) apply("<s>", "</s>");
				ImGui::SameLine();
				ImGui::ColorEdit3("##FmtColor", pick, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
				ImGui::SameLine();
				if (ImGui::SmallButton("Color")) {
					char tag[32];
					snprintf(tag, sizeof(tag), "<color=#%02X%02X%02X>",
						(int)(pick[0] * 255.0f + 0.5f), (int)(pick[1] * 255.0f + 0.5f), (int)(pick[2] * 255.0f + 0.5f));
					apply(tag, "</color>");
				}
				return hit;
			},
			[&] { text = pending; });

		ImGui::TextDisabled("Tags: <b> <i> <u> <s> <color=#RRGGBB[AA]|name> <size=N>. Use \\< for a literal '<'.");

		if (ImGui::TreeNode("Font Variants##TextVariants")) {
			EditorField::FileScene(parent, "Bold ", "##TextFontB", boldFontPath, [&] {}, variantSpec);
			EditorField::FileScene(parent, "Italic ", "##TextFontI", italicFontPath, [&] {}, variantSpec);
			EditorField::FileScene(parent, "Bold Italic ", "##TextFontBI", boldItalicFontPath, [&] {}, variantSpec);
			ImGui::TextDisabled("Empty slots fall back to faux bold / italic.");
			ImGui::TreePop();
		}
	}

	EditorField::FileScene(parent, "Font ", "##TextFont", fontPath, [&] {}, fontSpec);
	if (!fontPath.empty() && !GUIEngine::getInstance().GetFont(fontPath))
		ImGui::TextDisabled("Could not load font, using default.");

	EditorField::InputFloatScene(parent, "Font Size ", "##TextSize", &fontSize, [&] {
		fontSize = std::max(fontSize, 1.0f);

		}, "%.1f");
	EditorField::ColorEdit4Scene(parent, "Color ", "##TextColor", &color, [&] {});

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
	target->textMode = textMode;
	target->boldFontPath = boldFontPath;
	target->italicFontPath = italicFontPath;
	target->boldItalicFontPath = boldItalicFontPath;
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
	w.Write((int)textMode);
	w.WriteString(boldFontPath);
	w.WriteString(italicFontPath);
	w.WriteString(boldItalicFontPath);
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
	textMode = (TextMode)r.Read<int>();
	boldFontPath = r.ReadString();
	italicFontPath = r.ReadString();
	boldItalicFontPath = r.ReadString();
	richCacheValid = false;
}