#include "../../../../Header Files/Core/Editor/Windows/DocsWindow.h"  
#include "../../../../Header Files/Core/Editor/EditorTheme.h"         
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <set>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace {

	bool HttpGet(const std::string& url, std::string& out) {
		out.clear();
#if defined(_WIN32)
		HINTERNET net = InternetOpenA("FusionEngine", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
		if (!net) return false;
		HINTERNET h = InternetOpenUrlA(net, url.c_str(), nullptr, 0,
			INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES, 0);
		if (!h) { InternetCloseHandle(net); return false; }

		DWORD code = 0, len = sizeof(code);
		HttpQueryInfoA(h, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, nullptr);

		char buf[8192];
		DWORD got = 0;
		while (InternetReadFile(h, buf, sizeof(buf), &got) && got > 0) out.append(buf, got);

		InternetCloseHandle(h);
		InternetCloseHandle(net);
		return code == 200 && !out.empty();
#else
		std::string cmd = "curl -sfL --max-time 15 \"" + url + "\"";
		FILE* p = popen(cmd.c_str(), "r");
		if (!p) return false;
		char buf[8192];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
		return pclose(p) == 0 && !out.empty();
#endif
	}

	std::string ToLower(std::string s) {
		for (char& c : s) c = (char)std::tolower((unsigned char)c);
		return s;
	}

	void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
		size_t p = 0;
		while ((p = s.find(from, p)) != std::string::npos) {
			s.replace(p, from.size(), to);
			p += to.size();
		}
	}

	void AppendUtf8(std::string& out, unsigned cp) {
		if (cp < 0x80) out += (char)cp;
		else if (cp < 0x800) {
			out += (char)(0xC0 | (cp >> 6));
			out += (char)(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000) {
			out += (char)(0xE0 | (cp >> 12));
			out += (char)(0x80 | ((cp >> 6) & 0x3F));
			out += (char)(0x80 | (cp & 0x3F));
		}
		else {
			out += (char)(0xF0 | (cp >> 18));
			out += (char)(0x80 | ((cp >> 12) & 0x3F));
			out += (char)(0x80 | ((cp >> 6) & 0x3F));
			out += (char)(0x80 | (cp & 0x3F));
		}
	}

	std::string DecodeEntities(const std::string& s) {
		struct Named { const char* name; unsigned cp; };
		static const Named table[] = {
			{"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
			{"nbsp", 0xA0}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"hellip", 0x2026},
			{"rarr", 0x2192}, {"larr", 0x2190}, {"ldquo", 0x201C}, {"rdquo", 0x201D},
			{"lsquo", 0x2018}, {"rsquo", 0x2019}, {"copy", 0xA9}, {"para", 0xB6},
			{"middot", 0xB7}, {"bull", 0x2022}
		};

		std::string out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); i++) {
			if (s[i] != '&') { out += s[i]; continue; }
			size_t semi = s.find(';', i);
			if (semi == std::string::npos || semi - i > 10 || semi == i + 1) { out += '&'; continue; }

			const std::string e = s.substr(i + 1, semi - i - 1);
			unsigned cp = 0;
			bool ok = false;
			if (e[0] == '#') {
				try {
					cp = (e.size() > 1 && (e[1] == 'x' || e[1] == 'X'))
						? (unsigned)std::stoul(e.substr(2), nullptr, 16)
						: (unsigned)std::stoul(e.substr(1));
					ok = cp != 0;
				}
				catch (...) { ok = false; }
			}
			else {
				for (const Named& n : table) if (e == n.name) { cp = n.cp; ok = true; break; }
			}

			if (ok) { AppendUtf8(out, cp); i = semi; }
			else out += '&';
		}
		return out;
	}

	std::string CleanText(std::string s) {
		s = DecodeEntities(s);
		ReplaceAll(s, "\xC2\xB6", "");     
		ReplaceAll(s, "\xC2\xA0", " ");   
#if IMGUI_VERSION_NUM < 19200
		ReplaceAll(s, "\xE2\x80\x94", "-");
		ReplaceAll(s, "\xE2\x80\x93", "-");
		ReplaceAll(s, "\xE2\x80\xA6", "...");
		ReplaceAll(s, "\xE2\x80\x99", "'");
		ReplaceAll(s, "\xE2\x80\x98", "'");
		ReplaceAll(s, "\xE2\x80\x9C", "\"");
		ReplaceAll(s, "\xE2\x80\x9D", "\"");
		ReplaceAll(s, "\xE2\x80\xA2", "*");
		ReplaceAll(s, "\xE2\x86\x92", "->");
		ReplaceAll(s, "\xE2\x86\x90", "<-");
#endif
		return s;
	}

	void RemoveBlocks(std::string& html, const std::string& tag) {
		std::string lower = ToLower(html);
		const std::string open = "<" + tag, close = "</" + tag + ">";
		size_t pos = 0;
		while ((pos = lower.find(open, pos)) != std::string::npos) {
			size_t end = lower.find(close, pos);
			end = (end == std::string::npos) ? html.size() : end + close.size();
			html.erase(pos, end - pos);
			lower.erase(pos, end - pos);
		}
	}

	std::string ExtractTitle(const std::string& html) {
		std::string lower = ToLower(html);
		size_t a = lower.find("<title>");
		size_t b = lower.find("</title>");
		if (a == std::string::npos || b == std::string::npos || b < a) return "";
		return html.substr(a + 7, b - a - 7);
	}

	std::string ExtractContent(const std::string& html) {
		size_t m = html.find("md-content__inner");
		if (m == std::string::npos) return html;
		size_t s = html.find('>', m);
		if (s == std::string::npos) return html;
		size_t e = html.find("</article>", s);
		return html.substr(s + 1, e == std::string::npos ? std::string::npos : e - s - 1);
	}

	std::string HtmlToText(std::string html) {
		RemoveBlocks(html, "script");
		RemoveBlocks(html, "style");
		RemoveBlocks(html, "nav");

		std::string out;
		out.reserve(html.size() / 2);
		bool inTag = false;
		for (size_t i = 0; i < html.size(); i++) {
			char c = html[i];
			if (c == '<') {
				inTag = true;
				std::string t = ToLower(html.substr(i + 1, 4));
				if (t.rfind("p", 0) == 0 || t.rfind("br", 0) == 0 || t.rfind("/p", 0) == 0 ||
					t.rfind("h", 0) == 0 || t.rfind("/h", 0) == 0 || t.rfind("li", 0) == 0 ||
					t.rfind("/li", 0) == 0 || t.rfind("div", 0) == 0 || t.rfind("/div", 0) == 0 ||
					t.rfind("tr", 0) == 0 || t.rfind("pre", 0) == 0 || t.rfind("/pre", 0) == 0)
					out += '\n';
				else
					out += ' ';
			}
			else if (c == '>') inTag = false;
			else if (!inTag) out += c;
		}

		std::string res;
		bool lastSpace = false, lastNl = false;
		for (char c : out) {
			if (c == '\n') {
				if (!lastNl) res += '\n';
				lastNl = true; lastSpace = false;
			}
			else if (c == ' ' || c == '\t' || c == '\r') {
				if (!lastSpace && !lastNl) res += ' ';
				lastSpace = true;
			}
			else { res += c; lastSpace = lastNl = false; }
		}
		return CleanText(res);
	}

	std::string AttrValue(const std::string& attrs, const std::string& name) {
		const std::string lower = ToLower(attrs);
		size_t pos = 0;
		while ((pos = lower.find(name, pos)) != std::string::npos) {
			const bool boundary = pos == 0 || std::isspace((unsigned char)lower[pos - 1]);
			size_t e = pos + name.size();
			while (e < lower.size() && lower[e] == ' ') e++;
			if (boundary && e < lower.size() && lower[e] == '=') {
				e++;
				while (e < lower.size() && lower[e] == ' ') e++;
				if (e >= attrs.size()) return "";
				const char q = attrs[e];
				if (q == '"' || q == '\'') {
					size_t end = attrs.find(q, e + 1);
					return attrs.substr(e + 1, end == std::string::npos ? std::string::npos : end - e - 1);
				}
				size_t end = attrs.find_first_of(" \t\r\n", e);
				return attrs.substr(e, end == std::string::npos ? std::string::npos : end - e);
			}
			pos += name.size();
		}
		return "";
	}

	std::string Origin(const std::string& url) {
		size_t s = url.find("://");
		if (s == std::string::npos) return "";
		size_t e = url.find('/', s + 3);
		return e == std::string::npos ? url : url.substr(0, e);
	}

	std::string Normalize(const std::string& url) {
		size_t s = url.find("://");
		if (s == std::string::npos) return url;
		size_t hostEnd = url.find('/', s + 3);
		if (hostEnd == std::string::npos) return url;

		std::string head = url.substr(0, hostEnd);
		std::string path = url.substr(hostEnd);

		std::vector<std::string> parts;
		std::stringstream ss(path);
		std::string seg;
		while (std::getline(ss, seg, '/')) {
			if (seg.empty() || seg == ".") continue;
			if (seg == "..") { if (!parts.empty()) parts.pop_back(); }
			else parts.push_back(seg);
		}
		std::string res = head;
		for (auto& p : parts) res += "/" + p;
		if (path.back() == '/') res += "/";
		return res;
	}

	std::string Resolve(const std::string& current, const std::string& href) {
		std::string h = href;
		size_t cut = h.find_first_of("#?");
		if (cut != std::string::npos) h = h.substr(0, cut);
		if (h.empty()) return "";
		if (h.rfind("mailto:", 0) == 0 || h.rfind("javascript:", 0) == 0 || h.rfind("data:", 0) == 0) return "";

		std::string full;
		if (h.rfind("http://", 0) == 0 || h.rfind("https://", 0) == 0) full = h;
		else if (h[0] == '/') full = Origin(current) + h;
		else full = current.substr(0, current.find_last_of('/') + 1) + h;
		return Normalize(full);
	}

	bool LooksLikePage(const std::string& url) {
		std::string last = url.substr(url.find_last_of('/') + 1);
		if (last.empty()) return true;
		size_t dot = last.find_last_of('.');
		if (dot == std::string::npos) return true;
		std::string ext = ToLower(last.substr(dot));
		return ext == ".html" || ext == ".htm";
	}

	std::vector<std::string> ExtractLinks(const std::string& html, const std::string& current) {
		std::vector<std::string> links;
		std::string lower = ToLower(html);
		size_t pos = 0;
		while ((pos = lower.find("href=", pos)) != std::string::npos) {
			pos += 5;
			if (pos >= html.size()) break;
			char q = html[pos];
			if (q != '"' && q != '\'') continue;
			size_t end = html.find(q, pos + 1);
			if (end == std::string::npos) break;
			std::string r = Resolve(current, html.substr(pos + 1, end - pos - 1));
			if (!r.empty()) links.push_back(r);
			pos = end;
		}
		return links;
	}

	enum class Face { Regular, Bold };

	struct FontScope {
#if IMGUI_VERSION_NUM >= 19200
		FontScope(Face face, float scale = 1.0f) {
			ImFont* f = face == Face::Bold ? EditorTheme::g_Bold : nullptr;
			ImGui::PushFont(f, scale == 1.0f ? 0.0f : ImGui::GetStyle().FontSizeBase * scale);
		}
		~FontScope() { ImGui::PopFont(); }
#else
		bool pushed = false;
		float scale = 1.0f;
		FontScope(Face face, float s = 1.0f) : scale(s) {
			ImFont* f = face == Face::Bold ? EditorTheme::g_Bold : nullptr;
			if (f) { ImGui::PushFont(f); pushed = true; }
			if (scale != 1.0f) ImGui::SetWindowFontScale(scale);
		}
		~FontScope() {
			if (scale != 1.0f) ImGui::SetWindowFontScale(1.0f);
			if (pushed) ImGui::PopFont();
		}
#endif
		FontScope(const FontScope&) = delete;
		FontScope& operator=(const FontScope&) = delete;
	};

	ImVec4 AdmonitionColor(int kind) {
		switch (kind) {
		case 1:  return ImVec4(0.95f, 0.65f, 0.25f, 1.0f);   // warning
		case 2:  return ImVec4(0.90f, 0.35f, 0.35f, 1.0f);   // danger
		case 3:  return ImVec4(0.35f, 0.80f, 0.45f, 1.0f);   // tip
		default: return ImVec4(0.30f, 0.55f, 0.95f, 1.0f);   // note
		}
	}
}

std::vector<DocsWindow::Block> DocsWindow::ParseBlocks(const std::string& html, const std::string& pageUrl) {
	struct ListLevel { bool ordered; int counter; };

	struct State {
		std::vector<Block> blocks;
		Block cur;
		bool curActive = false;
		int bold = 0, code = 0;
		std::vector<std::string> links;
		bool inPre = false;
		std::string preText;
		std::vector<ListLevel> lists;
		std::vector<int> containers;     // admonition kind per open div/details (-1 = plain)
		int pendingBullet = 0;
		bool inTable = false;
		Block table;
		TableRow row;
		Runs cell;

		int Admon() const {
			for (size_t i = containers.size(); i-- > 0;) if (containers[i] >= 0) return containers[i];
			return -1;
		}

		static void TrimRuns(Runs& runs) {
			while (!runs.empty()) {
				std::string& t = runs.back().text;
				while (!t.empty() && t.back() == ' ') t.pop_back();
				if (t.empty()) runs.pop_back(); else break;
			}
		}

		void Flush() {
			if (!curActive) return;
			curActive = false;
			TrimRuns(cur.runs);
			if (!cur.runs.empty()) blocks.push_back(std::move(cur));
			cur = Block();
		}

		void Begin(Block::Type t, int level = 0) {
			Flush();
			cur = Block();
			cur.type = t;
			cur.level = level;
			cur.indent = (int)lists.size();
			cur.bullet = pendingBullet;
			pendingBullet = 0;
			cur.admonition = Admon();
			curActive = true;
		}

		Runs& Target() {
			if (inTable) return cell;
			if (!curActive) Begin(Block::Type::Paragraph);
			return cur.runs;
		}

		void AddText(const std::string& t) {
			if (inPre) { preText += t; return; }

			std::string c;
			c.reserve(t.size());
			bool lastSpace = false;
			for (char ch : t) {
				if (ch == '\n' || ch == '\t' || ch == '\r' || ch == ' ') {
					if (!lastSpace) c += ' ';
					lastSpace = true;
				}
				else { c += ch; lastSpace = false; }
			}

			const bool atStart = inTable ? cell.empty() : (!curActive || cur.runs.empty());
			if (atStart) {
				size_t k = c.find_first_not_of(' ');
				c = (k == std::string::npos) ? "" : c.substr(k);
			}
			if (c.empty()) return;

			Runs& runs = Target();
			if (!runs.empty() && !runs.back().text.empty() && runs.back().text.back() == ' ' && c[0] == ' ')
				c.erase(0, 1);
			if (c.empty()) return;

			const std::string url = links.empty() ? std::string() : links.back();
			if (!runs.empty() && runs.back().bold == (bold > 0) && runs.back().code == (code > 0) && runs.back().url == url) {
				runs.back().text += c;
			}
			else {
				Run r;
				r.text = c;
				r.bold = bold > 0;
				r.code = code > 0;
				r.url = url;
				runs.push_back(std::move(r));
			}
		}
	};

	State s;
	const std::string content = ExtractContent(html);
	const size_t n = content.size();
	std::string skipTag;
	int skipDepth = 0;
	bool titleBold = false;
	size_t i = 0;

	while (i < n) {
		if (content[i] != '<') {
			size_t j = content.find('<', i);
			if (j == std::string::npos) j = n;
			if (skipDepth == 0) s.AddText(CleanText(content.substr(i, j - i)));
			i = j;
			continue;
		}

		if (content.compare(i, 4, "<!--") == 0) {
			size_t e = content.find("-->", i + 4);
			i = (e == std::string::npos) ? n : e + 3;
			continue;
		}

		size_t j = i + 1;
		char q = 0;
		for (; j < n; j++) {
			char c = content[j];
			if (q) { if (c == q) q = 0; }
			else if (c == '"' || c == '\'') q = c;
			else if (c == '>') break;
		}
		if (j >= n) break;

		std::string tag = content.substr(i + 1, j - i - 1);
		i = j + 1;

		const bool closing = !tag.empty() && tag[0] == '/';
		if (closing) tag.erase(0, 1);
		const bool selfClose = !tag.empty() && tag.back() == '/';
		size_t sp = tag.find_first_of(" \t\r\n/");
		const std::string name = ToLower(tag.substr(0, sp));
		const std::string attrs = sp == std::string::npos ? "" : tag.substr(sp);
		if (name.empty() || name[0] == '!') continue;

		if (skipDepth > 0) {
			if (name == skipTag) {
				if (closing) skipDepth--;
				else if (!selfClose) skipDepth++;
			}
			continue;
		}

		const std::string cls = closing ? "" : ToLower(AttrValue(attrs, "class"));

		if (!closing) {
			const bool skip = name == "script" || name == "style" || name == "svg" || name == "button"
				|| name == "nav" || (name == "a" && cls.find("headerlink") != std::string::npos);
			if (skip) {
				if (!selfClose) { skipTag = name; skipDepth = 1; }
				continue;
			}
		}

		if (name == "br") {
			s.AddText(" ");
		}
		else if (name == "hr") {
			if (!closing) {
				s.Flush();
				Block b;
				b.type = Block::Type::Rule;
				b.admonition = s.Admon();
				s.blocks.push_back(std::move(b));
			}
		}
		else if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') {
			if (!closing) s.Begin(Block::Type::Heading, name[1] - '0');
			else s.Flush();
		}
		else if (name == "p") {
			if (!closing) {
				if (!s.inTable) s.Flush();
				if (cls.find("admonition-title") != std::string::npos) { s.bold++; titleBold = true; }
			}
			else {
				if (titleBold) { s.bold--; titleBold = false; }
				if (!s.inTable) s.Flush();
			}
		}
		else if (name == "ul" || name == "ol") {
			s.Flush();
			if (!closing) s.lists.push_back({ name == "ol", 0 });
			else if (!s.lists.empty()) s.lists.pop_back();
			s.pendingBullet = 0;
		}
		else if (name == "li") {
			s.Flush();
			if (!closing && !s.lists.empty())
				s.pendingBullet = s.lists.back().ordered ? ++s.lists.back().counter : -1;
			else
				s.pendingBullet = 0;
		}
		else if (name == "pre") {
			if (!closing) {
				s.Flush();
				s.inPre = true;
				s.preText.clear();
			}
			else {
				s.inPre = false;
				ReplaceAll(s.preText, "\t", "    ");
				ReplaceAll(s.preText, "\r", "");
				while (!s.preText.empty() && (s.preText.back() == '\n' || s.preText.back() == ' ')) s.preText.pop_back();
				if (!s.preText.empty()) {
					Block b;
					b.type = Block::Type::Code;
					b.code = s.preText;
					b.indent = (int)s.lists.size();
					b.admonition = s.Admon();
					s.blocks.push_back(std::move(b));
				}
				s.preText.clear();
			}
		}
		else if (name == "code" || name == "kbd") {
			if (!closing) s.code++;
			else if (s.code > 0) s.code--;
		}
		else if (name == "strong" || name == "b") {
			if (!closing) s.bold++;
			else if (s.bold > 0) s.bold--;
		}
		else if (name == "a") {
			if (!closing) s.links.push_back(Resolve(pageUrl, DecodeEntities(AttrValue(attrs, "href"))));
			else if (!s.links.empty()) s.links.pop_back();
		}
		else if (name == "table") {
			if (!closing) {
				s.Flush();
				s.inTable = true;
				s.table = Block();
				s.table.type = Block::Type::Table;
				s.table.indent = (int)s.lists.size();
				s.table.admonition = s.Admon();
			}
			else {
				s.inTable = false;
				if (!s.table.rows.empty()) s.blocks.push_back(std::move(s.table));
				s.table = Block();
			}
		}
		else if (name == "tr") {
			if (s.inTable) {
				if (!closing) s.row = TableRow();
				else if (!s.row.cells.empty()) s.table.rows.push_back(std::move(s.row));
			}
		}
		else if (name == "th" || name == "td") {
			if (s.inTable) {
				if (!closing) {
					s.cell.clear();
					if (name == "th") s.row.header = true;
				}
				else {
					State::TrimRuns(s.cell);
					s.row.cells.push_back(std::move(s.cell));
					s.cell.clear();
				}
			}
		}
		else if (name == "summary") {
			if (!closing) { s.Flush(); s.bold++; }
			else { if (s.bold > 0) s.bold--; s.Flush(); }
		}
		else if (name == "div" || name == "details") {
			s.Flush();
			if (!closing) {
				int kind = -1;
				if (cls.find("admonition") != std::string::npos || name == "details") {
					kind = 0;
					auto has = [&](const char* w) { return cls.find(w) != std::string::npos; };
					if (has("warning") || has("caution") || has("attention")) kind = 1;
					else if (has("danger") || has("error") || has("failure") || has("bug")) kind = 2;
					else if (has("tip") || has("hint") || has("success") || has("example")) kind = 3;
				}
				s.containers.push_back(kind);
			}
			else if (!s.containers.empty()) {
				s.containers.pop_back();
			}
		}
		else if (name == "section" || name == "blockquote" || name == "dl" || name == "dt" || name == "dd"
			|| name == "article" || name == "figure" || name == "figcaption" || name == "header"
			|| name == "footer" || name == "aside" || name == "main") {
			if (!s.inTable) s.Flush();
		}
	}

	s.Flush();
	return std::move(s.blocks);
}

DocsWindow* DocsWindow::instance = nullptr;

DocsWindow::DocsWindow(std::string name) : EditorWindow(name) {
	this->name = name;
	instance = this;
}

DocsWindow::~DocsWindow() {
	StopCrawl();
	if (instance == this) instance = nullptr;
}

void DocsWindow::OpenUrl(const std::string& url) {
#if defined(_WIN32)
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
	std::string cmd = "open \"" + url + "\" >/dev/null 2>&1 &";
	std::system(cmd.c_str());
#else
	std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
	std::system(cmd.c_str());
#endif
}

void DocsWindow::Open() {
	if (!instance) return;
	instance->open = true;
	instance->focusNext = true;
	instance->StartCrawl();   
}

void DocsWindow::StartCrawl() {
	if (crawlStarted) return;
	crawlStarted = true;
	worker = std::thread([this] { Crawl(kDocsUrl); });
}

void DocsWindow::ReloadCrawl() {
	StopCrawl();
	{ std::lock_guard<std::mutex> lock(mtx); pages.clear(); status = "Reloading..."; }
	crawlStarted = true;
	worker = std::thread([this] { Crawl(kDocsUrl); });
}

void DocsWindow::StopCrawl() {
	cancel = true;
	if (worker.joinable()) worker.join();
	cancel = false;
}

void DocsWindow::Crawl(std::string base) {
	constexpr size_t kMaxPages = 300;
	std::deque<std::string> queue{ base };
	std::set<std::string> seen{ base };

	while (!queue.empty() && !cancel && seen.size() <= kMaxPages) {
		std::string url = queue.front();
		queue.pop_front();

		{
			std::lock_guard<std::mutex> lock(mtx);
			status = "Indexing (" + std::to_string(pages.size()) + " pages)...";
		}

		std::string html;
		if (!HttpGet(url, html)) continue;

		auto page = std::make_shared<Page>();
		page->url = url;
		page->blocks = ParseBlocks(html, url);
		page->text = HtmlToText(ExtractContent(html));
		page->lower = ToLower(page->text);

		for (const Block& b : page->blocks) {
			if (b.type != Block::Type::Heading) continue;
			for (const Run& r : b.runs) page->title += r.text;
			break;
		}
		if (page->title.empty()) page->title = CleanText(ExtractTitle(html));
		if (page->title.empty()) page->title = url;

		for (auto& link : ExtractLinks(html, url)) {
			if (link.rfind(base, 0) != 0 || !LooksLikePage(link)) continue;
			if (seen.insert(link).second) queue.push_back(link);
		}

		std::lock_guard<std::mutex> lock(mtx);
		pages.push_back(std::move(page));
	}

	std::lock_guard<std::mutex> lock(mtx);
	status = pages.empty()
		? "Could not load documentation (offline, or the site renders with JavaScript)."
		: "Ready (" + std::to_string(pages.size()) + " pages)";
}

// ----- index queries -----

int DocsWindow::PageCount() {
	std::lock_guard<std::mutex> lock(mtx);
	return (int)pages.size();
}

std::string DocsWindow::Status() {
	std::lock_guard<std::mutex> lock(mtx);
	return status;
}

std::shared_ptr<DocsWindow::Page> DocsWindow::FindPage(const std::string& url) {
	std::lock_guard<std::mutex> lock(mtx);
	for (auto& p : pages) if (p->url == url) return p;
	return nullptr;
}

std::vector<DocsWindow::Result> DocsWindow::Search(const std::string& queryStr, int maxResults) {
	std::vector<Result> found;

	std::vector<std::string> terms;
	{
		std::stringstream ss(ToLower(queryStr));
		std::string t;
		while (ss >> t) terms.push_back(t);
	}

	std::lock_guard<std::mutex> lock(mtx);

	if (terms.empty()) {
		for (auto& p : pages) {
			Result r;
			r.page = p;
			found.push_back(std::move(r));
			if ((int)found.size() >= maxResults) break;
		}
		return found;
	}

	for (auto& p : pages) {
		const std::string titleLower = ToLower(p->title);
		int score = 0;
		bool all = true;
		size_t firstHit = std::string::npos;

		for (auto& t : terms) {
			size_t inTitle = titleLower.find(t);
			size_t inText = p->lower.find(t);
			if (inTitle == std::string::npos && inText == std::string::npos) { all = false; break; }
			if (inTitle != std::string::npos) score += 20;
			if (inText != std::string::npos) {
				int count = 0;
				for (size_t pos = inText; pos != std::string::npos && count < 20; pos = p->lower.find(t, pos + t.size())) count++;
				score += count;
				if (firstHit == std::string::npos || inText < firstHit) firstHit = inText;
			}
		}
		if (!all) continue;

		Result r;
		r.page = p;
		r.score = score;
		if (firstHit != std::string::npos) {
			size_t start = firstHit > 60 ? firstHit - 60 : 0;
			r.snippet = p->text.substr(start, 160);
			std::replace(r.snippet.begin(), r.snippet.end(), '\n', ' ');
			if (start > 0) r.snippet = "..." + r.snippet;
			r.snippet += "...";
		}
		found.push_back(std::move(r));
	}

	std::sort(found.begin(), found.end(), [](const Result& a, const Result& b) { return a.score > b.score; });
	if ((int)found.size() > maxResults) found.resize(maxResults);
	return found;
}

void DocsWindow::SelectPage(const std::shared_ptr<Page>& page, bool pushHistory) {
	if (!page || page == selectedPage) return;
	if (pushHistory && selectedPage) history.push_back(selectedPage->url);
	selectedPage = page;
	heights.clear();
	scrollToTop = true;
}

void DocsWindow::OpenLink(const std::string& url) {
	if (auto p = FindPage(url)) SelectPage(p, true);
	else OpenUrl(url);   
}

void DocsWindow::ResetView() {
	results.clear();
	selectedPage.reset();
	history.clear();
	heights.clear();
	autoSelected = false;
	lastPageCount = -1;
}


void DocsWindow::DrawRuns(const Runs& runs, float wrap, bool forceBold, std::string& clicked) {
	wrap = std::max(wrap, 50.0f);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const bool windowHovered = ImGui::IsWindowHovered();
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
	const ImU32 linkCol = ImGui::GetColorU32(EditorTheme::Accent());
	const ImU32 codeBg = ImGui::GetColorU32(ImGuiCol_Button);
	const float lineH = ImGui::GetTextLineHeight();
	const float lineStep = lineH + 3.0f;

	float x = 0.0f, y = 0.0f;
	bool gap = false;

	for (const Run& r : runs) {
		FontScope font((r.bold || forceBold) ? Face::Bold : Face::Regular);
		const float spaceW = ImGui::CalcTextSize(" ").x;

		const char* p = r.text.c_str();
		const char* end = p + r.text.size();
		while (p < end) {
			if (*p == ' ') { gap = true; p++; continue; }

			const char* w = p;
			while (p < end && *p != ' ') p++;

			const float ww = ImGui::CalcTextSize(w, p).x;
			float sp = (gap && x > 0.0f) ? spaceW : 0.0f;
			if (x > 0.0f && x + sp + ww > wrap) { x = 0.0f; y += lineStep; sp = 0.0f; }
			x += sp;
			gap = false;

			const ImVec2 pos(origin.x + x, origin.y + y);
			ImU32 col = textCol;

			if (r.code)
				dl->AddRectFilled(ImVec2(pos.x - 2.0f, pos.y), ImVec2(pos.x + ww + 2.0f, pos.y + lineH), codeBg, 3.0f);

			if (!r.url.empty()) {
				col = linkCol;
				const ImVec2 mx(pos.x + ww, pos.y + lineH);
				if (windowHovered && mouse.x >= pos.x && mouse.x < mx.x && mouse.y >= pos.y && mouse.y < mx.y) {
					ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					dl->AddLine(ImVec2(pos.x, mx.y - 1.0f), ImVec2(mx.x, mx.y - 1.0f), linkCol, 1.0f);
					if (ImGui::IsMouseClicked(0)) clicked = r.url;
				}
			}

			dl->AddText(pos, col, w, p);
			x += ww;
		}
	}

	ImGui::Dummy(ImVec2(wrap, y + lineH));
}

void DocsWindow::DrawBlock(const Block& b, int idx, std::string& clicked) {
	ImGui::PushID(idx);
	ImDrawList* dl = ImGui::GetWindowDrawList();

	const float indentStep = 22.0f;
	const float admonPad = b.admonition >= 0 ? 12.0f : 0.0f;
	const bool noIndent = b.type == Block::Type::Heading || b.type == Block::Type::Rule;
	const float ind = noIndent ? 0.0f : b.indent * indentStep;
	const ImVec2 start = ImGui::GetCursorScreenPos();

	if (admonPad > 0.0f) ImGui::Indent(admonPad);
	if (ind > 0.0f) ImGui::Indent(ind);

	switch (b.type) {
	case Block::Type::Heading: {
		static const float scales[] = { 1.0f, 1.7f, 1.4f, 1.2f, 1.05f, 1.0f, 1.0f };
		const int lv = std::clamp(b.level, 1, 6);
		if (lv <= 2) ImGui::Dummy(ImVec2(0.0f, 6.0f));
		FontScope font(Face::Bold, scales[lv]);
		DrawRuns(b.runs, ImGui::GetContentRegionAvail().x, true, clicked);
		break;
	}

	case Block::Type::Paragraph: {
		if (b.bullet != 0) {
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float lineH = ImGui::GetTextLineHeight();
			const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
			if (b.bullet < 0) {
				dl->AddCircleFilled(ImVec2(p.x - 11.0f, p.y + lineH * 0.5f), 2.5f, col);
			}
			else {
				char buf[16];
				std::snprintf(buf, sizeof(buf), "%d.", b.bullet);
				const ImVec2 ts = ImGui::CalcTextSize(buf);
				dl->AddText(ImVec2(p.x - 6.0f - ts.x, p.y), col, buf);
			}
		}
		DrawRuns(b.runs, ImGui::GetContentRegionAvail().x, false, clicked);
		break;
	}

	case Block::Type::Code: {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.058f, 0.068f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		if (ImGui::BeginChild("##code", ImVec2(0.0f, 0.0f),
			ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_HorizontalScrollbar)) {
			ImGui::TextUnformatted(b.code.c_str());
		}
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
		break;
	}

	case Block::Type::Table: {
		int cols = 0;
		for (const TableRow& row : b.rows) cols = std::max(cols, (int)row.cells.size());
		if (cols == 0) break;

		std::vector<float> weight(cols, 10.0f);
		for (const TableRow& row : b.rows) {
			for (size_t c = 0; c < row.cells.size(); c++) {
				size_t len = 0;
				for (const Run& r : row.cells[c]) len += r.text.size();
				weight[c] = std::max(weight[c], std::min(80.0f, (float)len));
			}
		}

		if (ImGui::BeginTable("##tbl", cols,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
			for (int c = 0; c < cols; c++)
				ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, weight[c]);

			for (const TableRow& row : b.rows) {
				ImGui::TableNextRow();
				if (row.header)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
				for (size_t c = 0; c < row.cells.size(); c++) {
					ImGui::TableSetColumnIndex((int)c);
					DrawRuns(row.cells[c], ImGui::GetContentRegionAvail().x, row.header, clicked);
				}
			}
			ImGui::EndTable();
		}
		break;
	}

	case Block::Type::Rule:
		ImGui::Separator();
		break;
	}

	if (ind > 0.0f) ImGui::Unindent(ind);
	if (admonPad > 0.0f) {
		ImGui::Unindent(admonPad);
		dl->AddRectFilled(ImVec2(start.x, start.y), ImVec2(start.x + 3.0f, ImGui::GetCursorScreenPos().y),
			ImGui::GetColorU32(AdmonitionColor(b.admonition)), 1.5f);
	}

	ImGui::PopID();
}

void DocsWindow::DrawPage(const Page& page, std::string& clicked) {
	const float width = ImGui::GetContentRegionAvail().x;
	if (heights.size() != page.blocks.size() || std::fabs(heightsWidth - width) > 0.5f) {
		heights.assign(page.blocks.size(), -1.0f);
		heightsWidth = width;
	}

	const float winTop = ImGui::GetWindowPos().y;
	const float winBottom = winTop + ImGui::GetWindowHeight();
	const float margin = 300.0f;
	const float spacing = ImGui::GetStyle().ItemSpacing.y;

	for (size_t i = 0; i < page.blocks.size(); i++) {
		const float startY = ImGui::GetCursorPosY();
		const float screenY = ImGui::GetCursorScreenPos().y;
		const float h = heights[i];

		if (h >= 0.0f && (screenY + h < winTop - margin || screenY > winBottom + margin)) {
			ImGui::Dummy(ImVec2(0.0f, std::max(0.0f, h - spacing)));
			continue;
		}

		DrawBlock(page.blocks[i], (int)i, clicked);
		heights[i] = ImGui::GetCursorPosY() - startY;
	}
}

void DocsWindow::ProcessWindow() {
	if (!open || hidden) return;

	if (focusNext) {
		ImGui::SetNextWindowFocus();
		focusNext = false;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(1000, 620), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(560, 320), ImVec2(FLT_MAX, FLT_MAX));

	bool visible = ImGui::Begin(name.c_str(), &open, ImGuiWindowFlags_NoDocking);
	if (!visible) {
		ImGui::End();
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();

	const int pageCount = PageCount();
	if (lastQuery != query || lastPageCount != pageCount) {
		results = Search(query);
		lastQuery = query;
		lastPageCount = pageCount;
	}

	if (!autoSelected && !selectedPage) {
		if (auto home = FindPage(kDocsUrl)) {
			SelectPage(home, false);
			autoSelected = true;
		}
	}

	const float reloadW = ImGui::CalcTextSize("Reload").x + style.FramePadding.x * 2.0f;
	ImGui::SetNextItemWidth(-(reloadW + style.ItemSpacing.x));
	ImGui::InputTextWithHint("##DocsQuery", "Search the documentation...", query, IM_ARRAYSIZE(query));
	ImGui::SameLine();
	if (ImGui::Button("Reload")) {
		ReloadCrawl();
		ResetView();
	}

	ImGui::TextDisabled("%s", Status().c_str());

	const float footerH = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f + 6.0f;
	const float listW = std::max(200.0f, ImGui::GetContentRegionAvail().x * 0.28f);

	ImGui::BeginChild("##DocsResults", ImVec2(listW, -footerH), ImGuiChildFlags_Borders);
	for (size_t i = 0; i < results.size(); i++) {
		const Result& r = results[i];
		ImGui::PushID((int)i);
		if (ImGui::Selectable(r.page->title.c_str(), r.page == selectedPage)) {
			SelectPage(r.page, true);
		}
		if (!r.snippet.empty()) {
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", r.snippet.c_str());
			ImGui::PopTextWrapPos();
			ImGui::Separator();
		}
		ImGui::PopID();
	}
	if (results.empty() && query[0] != '\0') ImGui::TextDisabled("No results.");
	ImGui::EndChild();

	ImGui::SameLine();

	std::string clicked;
	ImGui::BeginChild("##DocsReader", ImVec2(0, -footerH), ImGuiChildFlags_Borders);
	if (!selectedPage) {
		ImGui::TextDisabled("Select a page to read it here.");
	}
	else {
		if (scrollToTop) { ImGui::SetScrollY(0.0f); scrollToTop = false; }
		DrawPage(*selectedPage, clicked);
	}
	ImGui::EndChild();

	if (!clicked.empty()) OpenLink(clicked);

	ImGui::Separator();

	ImGui::BeginDisabled(history.empty());
	if (ImGui::Button("Back")) {
		std::string url = history.back();
		history.pop_back();
		if (auto p = FindPage(url)) SelectPage(p, false);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!selectedPage);
	if (ImGui::Button("Open in Browser") && selectedPage) OpenUrl(selectedPage->url);
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100.0f - style.WindowPadding.x);
	if (ImGui::Button("Close", ImVec2(100, 0))) open = false;

	ImGui::End();
}