#include "../../../Header Files/Components/GUI/TextComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
	EditorField::FileSpec MakeFontSpec(const char* hint) {
		EditorField::FileSpec s;
		s.dialog.title = "Choose Font";
		s.dialog.filters = { { "Fonts", "*.ttf;*.otf" }, { "All Files", "*.*" } };
		s.extensions = { ".ttf", ".otf" };
		s.hint = hint;
		s.storeVirtual = true;
		s.displayVirtual = true;
		s.allowClear = true;
		return s;
	}

	struct TagOp {
		char kind = 0;          
		bool close = false;
		glm::vec4 col{ 1.0f };
		float size = 0.0f;
	};

	bool ParseColor(const std::string& v, glm::vec4& out) {
		struct Named { const char* n; float r, g, b; };
		static const Named names[] = {
			{"white",1,1,1}, {"black",0,0,0}, {"red",1,0,0}, {"green",0,0.8f,0},
			{"blue",0,0.4f,1}, {"yellow",1,0.92f,0}, {"cyan",0,1,1},
			{"magenta",1,0,1}, {"orange",1,0.55f,0}, {"gray",0.5f,0.5f,0.5f},
			{"grey",0.5f,0.5f,0.5f}, {"purple",0.6f,0.2f,0.8f}, {"pink",1,0.5f,0.75f}
		};
		if (v.empty()) return false;
		if (v[0] == '#') {
			const size_t n = v.size() - 1;
			if (n != 6 && n != 8) return false;
			for (size_t i = 1; i < v.size(); ++i)
				if (!std::isxdigit((unsigned char)v[i])) return false;
			auto hex = [&](size_t i) { return std::strtoul(v.substr(i, 2).c_str(), nullptr, 16) / 255.0f; };
			out = glm::vec4(hex(1), hex(3), hex(5), n == 8 ? hex(7) : 1.0f);
			return true;
		}
		for (const Named& nm : names) {
			if (v == nm.n) { out = glm::vec4(nm.r, nm.g, nm.b, 1.0f); return true; }
		}
		return false;
	}

	bool ParseTag(const std::string& raw, TagOp& op) {
		std::string t;
		t.reserve(raw.size());
		for (char ch : raw) t += (char)std::tolower((unsigned char)ch);

		op = TagOp();
		size_t i = 0;
		if (!t.empty() && t[0] == '/') { op.close = true; i = 1; }

		const size_t eq = t.find('=', i);
		const std::string name = (eq == std::string::npos) ? t.substr(i) : t.substr(i, eq - i);
		const std::string val = (eq == std::string::npos) ? "" : t.substr(eq + 1);

		if ((name == "b" || name == "i" || name == "u" || name == "s") && val.empty()) {
			op.kind = name[0];
			return true;
		}
		if (name == "color") {
			op.kind = 'c';
			if (!op.close && !ParseColor(val, op.col)) return false;
			return true;
		}
		if (name == "size") {
			op.kind = 'z';
			if (!op.close) {
				char* end = nullptr;
				const float v = std::strtof(val.c_str(), &end);
				if (end == val.c_str() || v <= 0.0f) return false;
				op.size = v;
			}
			return true;
		}
		return false;
	}

	int sSelA = 0, sSelB = 0;

	int SelectionTracker(ImGuiInputTextCallbackData* d) {
		sSelA = d->SelectionStart;
		sSelB = d->SelectionEnd;
		return 0;
	}

	std::string WrapSelection(const std::string& src, const std::string& open, const std::string& close) {
		size_t a = (size_t)std::max(0, std::min(sSelA, sSelB));
		size_t b = (size_t)std::max(0, std::max(sSelA, sSelB));
		a = std::min(a, src.size());
		b = std::min(b, src.size());
		std::string out = src.substr(0, a) + open + src.substr(a, b - a) + close + src.substr(b);
		sSelA = sSelB = (int)(a + open.size());   
		return out;
	}

	inline const char* NextUtf8(const char* p, const char* end) {
		++p;
		while (p < end && ((unsigned char)*p & 0xC0) == 0x80) ++p;
		return p;
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

void TextComponent::ParseRich() {
	richCache.clear();
	richCacheSrc = text;
	richCacheRich = IsRich();
	richCacheValid = true;

	int bold = 0, italic = 0, underline = 0, strike = 0;
	std::vector<glm::vec4> colorStack;
	std::vector<float> sizeStack;
	std::string run;

	auto emit = [&]() {
		size_t i = 0;
		while (i < run.size()) {
			RichPiece p;
			p.bold = bold > 0;
			p.italic = italic > 0;
			p.underline = underline > 0;
			p.strike = strike > 0;
			p.hasColor = !colorStack.empty();
			if (p.hasColor) p.color = colorStack.back();
			p.size = sizeStack.empty() ? 0.0f : sizeStack.back();

			const char ch = run[i];
			if (ch == '\r') { ++i; continue; }
			if (ch == '\n') {
				p.newline = true;
				++i;
			}
			else if (ch == ' ' || ch == '\t') {
				p.space = true;
				while (i < run.size() && (run[i] == ' ' || run[i] == '\t')) { p.text += run[i]; ++i; }
			}
			else {
				while (i < run.size() && run[i] != ' ' && run[i] != '\t' && run[i] != '\n' && run[i] != '\r') {
					p.text += run[i]; ++i;
				}
			}
			richCache.push_back(std::move(p));
		}
		run.clear();
		};

	for (size_t k = 0; k < text.size();) {
		const char ch = text[k];

		if (IsRich() && ch == '\\' && k + 1 < text.size() && text[k + 1] == '<') {  
			run += '<';
			k += 2;
			continue;
		}

		if (IsRich() && ch == '<') {
			const size_t close = text.find('>', k);
			TagOp op;
			if (close != std::string::npos && ParseTag(text.substr(k + 1, close - k - 1), op)) {
				emit();   
				switch (op.kind) {
				case 'b': bold = std::max(0, bold + (op.close ? -1 : 1)); break;
				case 'i': italic = std::max(0, italic + (op.close ? -1 : 1)); break;
				case 'u': underline = std::max(0, underline + (op.close ? -1 : 1)); break;
				case 's': strike = std::max(0, strike + (op.close ? -1 : 1)); break;
				case 'c':
					if (op.close) { if (!colorStack.empty()) colorStack.pop_back(); }
					else colorStack.push_back(op.col);
					break;
				case 'z':
					if (op.close) { if (!sizeStack.empty()) sizeStack.pop_back(); }
					else sizeStack.push_back(op.size);
					break;
				}
				k = close + 1;
				continue;
			}
		}

		run += ch;
		++k;
	}
	emit();
}

void TextComponent::DrawUI(const UIDrawContext& c) {
	if (text.empty() || color.a <= 0.0f || fontSize <= 0.0f) return;

	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	if (w <= 0.0f || h <= 0.0f) return;

	if (!richCacheValid || richCacheRich != IsRich() || richCacheSrc != text) ParseRich();

	struct FontSel { ImFont* font; bool fakeBold; bool fakeItalic; };
	auto load = [](const std::string& path) -> ImFont* {
		return path.empty() ? nullptr : GUIEngine::getInstance().GetFont(path);
		};
	ImFont* reg = load(fontPath);
	if (!reg) reg = ImGui::GetFont();
	ImFont* bf = load(boldFontPath);
	ImFont* itf = load(italicFontPath);
	ImFont* bif = load(boldItalicFontPath);

	FontSel fonts[4];
	fonts[0] = { reg, false, false };
	fonts[1] = { bf ? bf : reg, bf == nullptr, false };
	fonts[2] = { itf ? itf : reg, false, itf == nullptr };
	if (bif)      fonts[3] = { bif, false, false };
	else if (bf)  fonts[3] = { bf, false, true };
	else if (itf) fonts[3] = { itf, true, false };
	else          fonts[3] = { reg, true, true };

	auto boldOffset = [](float px) { return std::max(0.5f, px * 0.035f); };
	auto fontIndex = [](const RichPiece& p) { return (p.bold ? 1 : 0) | (p.italic ? 2 : 0); };
	auto measure = [&](const char* b, const char* e, float px, int fi) {
		float mw = fonts[fi].font->CalcTextSizeA(px, FLT_MAX, 0.0f, b, e).x;
		if (fonts[fi].fakeBold) mw += boldOffset(px);
		return mw;
		};

	struct Placed {
		const RichPiece* p;
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
		while (!cur.items.empty() && cur.items.back().p->space) {   
			cur.width -= cur.items.back().w;
			cur.items.pop_back();
		}
		if (cur.height <= 0.0f) cur.height = basePx;
		lines.push_back(std::move(cur));
		cur = Line();
		};
	auto addItem = [&](const RichPiece* p, const char* b, const char* e, float iw, float px, int fi) {
		cur.items.push_back({ p, b, e, cur.width, iw, px, fi });
		cur.width += iw;
		cur.height = std::max(cur.height, px);
		};

	for (const RichPiece& p : richCache) {
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
		if (p.space && cur.items.empty() && wrapped) continue;  

		if (wrap > 0.0f && !p.space && pw > wrap) {
			const char* s = b;
			while (s < e) {
				const float avail = wrap - cur.width;
				const char* q = s;
				float acc = 0.0f;
				while (q < e) {
					const char* n = NextUtf8(q, e);
					const float cw = measure(q, n, px, fi);
					if (acc + cw > avail && q > s) break;
					if (acc + cw > avail && q == s) break;
					acc += cw;
					q = n;
				}
				if (q == s) {
					if (!cur.items.empty()) { flush(); wrapped = true; continue; }
					q = NextUtf8(s, e);                      
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
			const RichPiece& p = *it.p;
			if (p.newline) continue;

			glm::vec4 cc = color;
			if (p.hasColor) cc = glm::vec4(p.color.r, p.color.g, p.color.b, p.color.a * color.a);
			if (cc.a <= 0.0f) continue;
			const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(cc.r, cc.g, cc.b, cc.a));

			const float x = x0 + it.x;
			const float ty = y + (line.height - it.px);   
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

void TextComponent::ProcessInspectorUI() {
	static const char* hNames[] = { "Left", "Center", "Right" };
	static const char* vNames[] = { "Top", "Middle", "Bottom" };
	static const EditorField::FileSpec fontSpec = MakeFontSpec("Default (click to choose...)");
	static const EditorField::FileSpec variantSpec = MakeFontSpec("None (faux style)");

	char buf[4096];
	strncpy_s(buf, sizeof(buf), text.c_str(), _TRUNCATE);
	EditorField::Wrap(parent, "Text ",
		[&] {
			return ImGui::InputTextMultiline("##TextStr", buf, sizeof(buf), ImVec2(-FLT_MIN, 90.0f),
				ImGuiInputTextFlags_CallbackAlways, SelectionTracker, nullptr);
		},
		[&] { text = buf; });

	static const char* modeNames[] = { "Plain", "Rich Text" };
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
				if (ImGui::SmallButton("B")) { pending = WrapSelection(text, "<b>", "</b>"); hit = true; }
				ImGui::SameLine();
				if (ImGui::SmallButton("I")) { pending = WrapSelection(text, "<i>", "</i>"); hit = true; }
				ImGui::SameLine();
				if (ImGui::SmallButton("U")) { pending = WrapSelection(text, "<u>", "</u>"); hit = true; }
				ImGui::SameLine();
				if (ImGui::SmallButton("S")) { pending = WrapSelection(text, "<s>", "</s>"); hit = true; }
				ImGui::SameLine();
				ImGui::ColorEdit3("##FmtColor", pick, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
				ImGui::SameLine();
				if (ImGui::SmallButton("Color")) {
					char tag[32];
					snprintf(tag, sizeof(tag), "<color=#%02X%02X%02X>",
						(int)(pick[0] * 255.0f + 0.5f), (int)(pick[1] * 255.0f + 0.5f), (int)(pick[2] * 255.0f + 0.5f));
					pending = WrapSelection(text, tag, "</color>");
					hit = true;
				}
				return hit;
			},
			[&] { text = pending; });

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