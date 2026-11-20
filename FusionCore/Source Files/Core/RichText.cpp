#include "../../Header Files/Core/RichText.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

bool RichText::ParseColor(const std::string& v, glm::vec4& out) {
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

bool RichText::ParseTag(const std::string& raw, Tag& op) {
	std::string t;
	t.reserve(raw.size());
	for (char ch : raw) t += (char)std::tolower((unsigned char)ch);

	op = Tag();
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
		if (!op.close && !ParseColor(val, op.color)) return false;
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

std::vector<RichText::Piece> RichText::Parse(const std::string& text, bool enableTags) {
	std::vector<Piece> pieces;

	int bold = 0, italic = 0, underline = 0, strike = 0;
	std::vector<glm::vec4> colorStack;
	std::vector<float> sizeStack;
	std::string run;

	auto emit = [&]() {
		size_t i = 0;
		while (i < run.size()) {
			Piece p;
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
			pieces.push_back(std::move(p));
		}
		run.clear();
		};

	for (size_t k = 0; k < text.size();) {
		const char ch = text[k];

		if (enableTags && ch == '\\' && k + 1 < text.size() && text[k + 1] == '<') {   
			run += '<';
			k += 2;
			continue;
		}

		if (enableTags && ch == '<') {
			const size_t close = text.find('>', k);
			Tag op;
			if (close != std::string::npos && ParseTag(text.substr(k + 1, close - k - 1), op)) {
				emit();   
				switch (op.kind) {
				case 'b': bold = std::max(0, bold + (op.close ? -1 : 1)); break;
				case 'i': italic = std::max(0, italic + (op.close ? -1 : 1)); break;
				case 'u': underline = std::max(0, underline + (op.close ? -1 : 1)); break;
				case 's': strike = std::max(0, strike + (op.close ? -1 : 1)); break;
				case 'c':
					if (op.close) { if (!colorStack.empty()) colorStack.pop_back(); }
					else colorStack.push_back(op.color);
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
	return pieces;
}

std::string RichText::WrapSelection(const std::string& src, int selStart, int selEnd,
	const std::string& open, const std::string& close, int* newCaret) {
	size_t a = (size_t)std::max(0, std::min(selStart, selEnd));
	size_t b = (size_t)std::max(0, std::max(selStart, selEnd));
	a = std::min(a, src.size());
	b = std::min(b, src.size());
	if (newCaret) *newCaret = (int)(a + open.size());   
	return src.substr(0, a) + open + src.substr(a, b - a) + close + src.substr(b);
}

const char* RichText::NextUtf8(const char* p, const char* end) {
	++p;
	while (p < end && ((unsigned char)*p & 0xC0) == 0x80) ++p;
	return p;
}