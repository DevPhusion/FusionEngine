#pragma once
#include "../../../imgui/imgui.h"
#include "../../../imgui/imgui_internal.h"
#include "Windows/Console.h"

namespace EditorTheme {

	struct Theme {
		std::string name = "Default";

		ImVec4 accent{ 0.30f, 0.78f, 0.64f, 1.0f };
		ImVec4 accentDim{ 0.20f, 0.52f, 0.43f, 1.0f };
		ImVec4 danger{ 0.90f, 0.30f, 0.35f, 1.0f };

		ImVec4 text{ 0.90f, 0.90f, 0.90f, 1.0f };
		ImVec4 textDisabled{ 0.50f, 0.50f, 0.50f, 1.0f };

		ImVec4 bg0{ 0.075f, 0.075f, 0.075f, 1.0f };   // title bars, tabs, menu bar
		ImVec4 bg1{ 0.100f, 0.100f, 0.100f, 1.0f };   // window background
		ImVec4 bg2{ 0.120f, 0.120f, 0.120f, 1.0f };   // child / popup background
		ImVec4 fill0{ 0.150f, 0.150f, 0.150f, 1.0f }; // input fields
		ImVec4 fill1{ 0.200f, 0.200f, 0.200f, 1.0f }; // buttons, hovered inputs
		ImVec4 fill2{ 0.250f, 0.250f, 0.250f, 1.0f }; // hovered buttons, active inputs
		ImVec4 border{ 0.215f, 0.215f, 0.215f, 1.0f };

		ImVec4 axisX{ 214 / 255.f, 92 / 255.f, 108 / 255.f, 1.0f };
		ImVec4 axisY{ 120 / 255.f, 190 / 255.f, 120 / 255.f, 1.0f };
		ImVec4 axisZ{ 98 / 255.f, 156 / 255.f, 220 / 255.f, 1.0f };
	};

	inline Theme currentTheme;

	inline ImVec4 Accent() { return currentTheme.accent; }
	inline ImVec4 AccentDim() { return currentTheme.accentDim; }
	inline ImVec4 Danger() { return currentTheme.danger; }

	inline ImU32 AxisColor(int axis) {
		switch (axis) {
		case 0:  return ImGui::ColorConvertFloat4ToU32(currentTheme.axisX);
		case 1:  return ImGui::ColorConvertFloat4ToU32(currentTheme.axisY);
		case 2:  return ImGui::ColorConvertFloat4ToU32(currentTheme.axisZ);
		default: return IM_COL32(200, 160, 80, 255);
		}
	}

	inline float LabelColumnWidth() {
		float w = ImGui::GetContentRegionAvail().x * 0.36f;
		return w < 90.0f ? 90.0f : w;
	}

	inline ImFont* g_Bold = nullptr;

	inline void LoadFonts(float size = 16.0f) {
		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig cfg;
		cfg.OversampleH = 3;
		cfg.OversampleV = 2;

		ImFont* regular = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Inter-Regular.ttf", size, &cfg);
		if (regular) io.FontDefault = regular;
		else Console::PrintError("EditorTheme: Unable to load font Resources/Fonts/Inter-Regular.ttf");

		g_Bold = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Inter-SemiBold.ttf", size, &cfg);
		if (!g_Bold) Console::PrintError("EditorTheme: Unable to load font Resources/Fonts/Inter-SemiBold.ttf");
	}

	inline bool PushBold() {
		if (!g_Bold) return false;
#if IMGUI_VERSION_NUM >= 19200
		ImGui::PushFont(g_Bold, 0.0f);
#else
		ImGui::PushFont(g_Bold);
#endif
		return true;
	}
	inline void PopBold(bool pushed) { if (pushed) ImGui::PopFont(); }

	inline void ApplyDockClass(ImGuiDockNodeFlags extraOverride = 0) {
		static ImGuiWindowClass cls;
		cls.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton
			| ImGuiDockNodeFlags_NoCloseButton | extraOverride;
		ImGui::SetNextWindowClass(&cls);
	}

	inline void Apply() {
		ImGuiStyle& s = ImGui::GetStyle();
		const Theme& t = currentTheme;

		s.WindowPadding = ImVec2(12, 12);
		s.FramePadding = ImVec2(8, 5);
		s.ItemSpacing = ImVec2(8, 7);
		s.ItemInnerSpacing = ImVec2(6, 6);
		s.IndentSpacing = 14.0f;
		s.ScrollbarSize = 12.0f;
		s.GrabMinSize = 10.0f;

		s.WindowBorderSize = 1.0f;
		s.ChildBorderSize = 1.0f;
		s.PopupBorderSize = 1.0f;
		s.FrameBorderSize = 0.0f;
		s.TabBorderSize = 0.0f;

		s.WindowRounding = 6.0f;
		s.ChildRounding = 5.0f;
		s.FrameRounding = 4.0f;
		s.PopupRounding = 6.0f;
		s.ScrollbarRounding = 8.0f;
		s.GrabRounding = 4.0f;
		s.TabRounding = 4.0f;

		s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		s.SeparatorTextBorderSize = 1.0f;
		s.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
		s.SeparatorTextPadding = ImVec2(8.0f, 4.0f);

		auto A = [](const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); };
		auto G = [](float v, float a = 1.0f) { return ImVec4(v, v, v, a); };
		auto Mix = [](const ImVec4& a, const ImVec4& b, float k) {
			return ImVec4(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k,
				a.z + (b.z - a.z) * k, 1.0f);
			};

		const ImVec4 accent = t.accent;
		const ImVec4 accentDim = t.accentDim;

		const ImVec4 buttonActive = Mix(t.fill2, ImVec4(1, 1, 1, 1), 0.08f);
		const ImVec4 header = Mix(t.fill0, t.fill1, 0.5f);
		const ImVec4 headerHover = Mix(t.fill1, t.fill2, 0.3f);
		const ImVec4 sliderGrab = Mix(t.fill2, t.text, 0.33f);
		const ImVec4 scrollActive = Mix(t.fill2, t.text, 0.14f);
		const ImVec4 sepHover = Mix(t.fill2, t.text, 0.23f);
		const ImVec4 navCursor = Mix(t.fill2, t.text, 0.54f);

		ImVec4* c = s.Colors;
		c[ImGuiCol_Text] = t.text;
		c[ImGuiCol_TextDisabled] = t.textDisabled;
		c[ImGuiCol_WindowBg] = t.bg1;
		c[ImGuiCol_ChildBg] = t.bg2;
		c[ImGuiCol_PopupBg] = t.bg2;
		c[ImGuiCol_Border] = t.border;
		c[ImGuiCol_BorderShadow] = G(0.0f, 0.0f);

		c[ImGuiCol_FrameBg] = t.fill0;
		c[ImGuiCol_FrameBgHovered] = t.fill1;
		c[ImGuiCol_FrameBgActive] = t.fill2;

		c[ImGuiCol_TitleBg] = t.bg0;
		c[ImGuiCol_TitleBgActive] = t.bg0;
		c[ImGuiCol_TitleBgCollapsed] = t.bg0;
		c[ImGuiCol_MenuBarBg] = t.bg0;

		c[ImGuiCol_ScrollbarBg] = G(0.0f, 0.0f);
		c[ImGuiCol_ScrollbarGrab] = t.fill1;
		c[ImGuiCol_ScrollbarGrabHovered] = t.fill2;
		c[ImGuiCol_ScrollbarGrabActive] = scrollActive;

		c[ImGuiCol_CheckMark] = accent;
		c[ImGuiCol_SliderGrab] = sliderGrab;
		c[ImGuiCol_SliderGrabActive] = accent;

		c[ImGuiCol_Button] = t.fill1;
		c[ImGuiCol_ButtonHovered] = t.fill2;
		c[ImGuiCol_ButtonActive] = buttonActive;

		c[ImGuiCol_Header] = header;
		c[ImGuiCol_HeaderHovered] = headerHover;
		c[ImGuiCol_HeaderActive] = t.fill2;

		c[ImGuiCol_Separator] = t.border;
		c[ImGuiCol_SeparatorHovered] = sepHover;
		c[ImGuiCol_SeparatorActive] = accent;

		c[ImGuiCol_ResizeGrip] = G(1.0f, 0.06f);
		c[ImGuiCol_ResizeGripHovered] = G(1.0f, 0.20f);
		c[ImGuiCol_ResizeGripActive] = A(accent, 0.80f);

		c[ImGuiCol_Tab] = t.bg0;
		c[ImGuiCol_TabHovered] = t.fill2;
		c[ImGuiCol_TabSelected] = t.fill1;
		c[ImGuiCol_TabSelectedOverline] = accent;
		c[ImGuiCol_TabDimmed] = t.bg0;
		c[ImGuiCol_TabDimmedSelected] = t.bg2;
		c[ImGuiCol_TabDimmedSelectedOverline] = accentDim;

		c[ImGuiCol_TableHeaderBg] = t.bg0;
		c[ImGuiCol_TableBorderStrong] = t.border;
		c[ImGuiCol_TableBorderLight] = A(t.border, 0.5f);
		c[ImGuiCol_TableRowBg] = G(0.0f, 0.0f);
		c[ImGuiCol_TableRowBgAlt] = G(1.0f, 0.02f);

		c[ImGuiCol_TextSelectedBg] = A(accent, 0.28f);
		c[ImGuiCol_DragDropTarget] = accent;
		c[ImGuiCol_NavCursor] = navCursor;
		c[ImGuiCol_NavWindowingHighlight] = G(1.0f, 0.6f);
		c[ImGuiCol_NavWindowingDimBg] = G(0.0f, 0.5f);
		c[ImGuiCol_ModalWindowDimBg] = G(0.0f, 0.55f);
#ifdef IMGUI_HAS_DOCK
		c[ImGuiCol_DockingPreview] = A(accent, 0.40f);
		c[ImGuiCol_DockingEmptyBg] = t.bg0;
#endif
	}
}