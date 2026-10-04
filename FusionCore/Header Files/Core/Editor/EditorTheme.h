#pragma once
#include "../../../imgui/imgui.h"
#include "../../../imgui/imgui_internal.h"
#include "Windows/Console.h"

namespace EditorTheme {

	inline ImVec4 Accent() { return ImVec4(0.30f, 0.78f, 0.64f, 1.00f); }
	inline ImVec4 AccentDim() { return ImVec4(0.20f, 0.52f, 0.43f, 1.00f); }
	inline ImVec4 Danger() { return ImVec4(0.90f, 0.30f, 0.35f, 1.00f); }

	inline ImU32 AxisColor(int axis) {
		switch (axis) {
		case 0:  return IM_COL32(214, 92, 108, 255);
		case 1:  return IM_COL32(120, 190, 120, 255);
		case 2:  return IM_COL32(98, 156, 220, 255);
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

		const ImVec4 accent = Accent();
		const ImVec4 accentDim = AccentDim();
		auto A = [](const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); };
		auto G = [](float v, float a = 1.0f) { return ImVec4(v, v, v, a); };

		const ImVec4 bg0 = G(0.075f);
		const ImVec4 bg1 = G(0.100f);
		const ImVec4 bg2 = G(0.120f);
		const ImVec4 fill0 = G(0.150f);
		const ImVec4 fill1 = G(0.200f);
		const ImVec4 fill2 = G(0.250f);
		const ImVec4 border = G(0.215f);

		ImVec4* c = s.Colors;
		c[ImGuiCol_Text] = G(0.90f);
		c[ImGuiCol_TextDisabled] = G(0.50f);
		c[ImGuiCol_WindowBg] = bg1;
		c[ImGuiCol_ChildBg] = bg2;
		c[ImGuiCol_PopupBg] = bg2;
		c[ImGuiCol_Border] = border;
		c[ImGuiCol_BorderShadow] = G(0.0f, 0.0f);

		c[ImGuiCol_FrameBg] = fill0;
		c[ImGuiCol_FrameBgHovered] = fill1;
		c[ImGuiCol_FrameBgActive] = fill2;

		c[ImGuiCol_TitleBg] = bg0;
		c[ImGuiCol_TitleBgActive] = bg0;
		c[ImGuiCol_TitleBgCollapsed] = bg0;
		c[ImGuiCol_MenuBarBg] = bg0;

		c[ImGuiCol_ScrollbarBg] = G(0.0f, 0.0f);
		c[ImGuiCol_ScrollbarGrab] = fill1;
		c[ImGuiCol_ScrollbarGrabHovered] = fill2;
		c[ImGuiCol_ScrollbarGrabActive] = G(0.34f);

		c[ImGuiCol_CheckMark] = accent;
		c[ImGuiCol_SliderGrab] = G(0.46f);
		c[ImGuiCol_SliderGrabActive] = accent;

		c[ImGuiCol_Button] = fill1;
		c[ImGuiCol_ButtonHovered] = fill2;
		c[ImGuiCol_ButtonActive] = G(0.31f);

		c[ImGuiCol_Header] = G(0.175f);
		c[ImGuiCol_HeaderHovered] = G(0.215f);
		c[ImGuiCol_HeaderActive] = G(0.255f);

		c[ImGuiCol_Separator] = border;
		c[ImGuiCol_SeparatorHovered] = G(0.40f);
		c[ImGuiCol_SeparatorActive] = accent;

		c[ImGuiCol_ResizeGrip] = G(1.0f, 0.06f);
		c[ImGuiCol_ResizeGripHovered] = G(1.0f, 0.20f);
		c[ImGuiCol_ResizeGripActive] = A(accent, 0.80f);

		c[ImGuiCol_Tab] = bg0;
		c[ImGuiCol_TabHovered] = fill2;
		c[ImGuiCol_TabSelected] = fill1;
		c[ImGuiCol_TabSelectedOverline] = accent;
		c[ImGuiCol_TabDimmed] = bg0;
		c[ImGuiCol_TabDimmedSelected] = bg2;
		c[ImGuiCol_TabDimmedSelectedOverline] = accentDim;

		c[ImGuiCol_TableHeaderBg] = bg0;
		c[ImGuiCol_TableBorderStrong] = border;
		c[ImGuiCol_TableBorderLight] = A(border, 0.5f);
		c[ImGuiCol_TableRowBg] = G(0.0f, 0.0f);
		c[ImGuiCol_TableRowBgAlt] = G(1.0f, 0.02f);

		c[ImGuiCol_TextSelectedBg] = A(accent, 0.28f);
		c[ImGuiCol_DragDropTarget] = accent;
		c[ImGuiCol_NavCursor] = G(0.60f);
		c[ImGuiCol_NavWindowingHighlight] = G(1.0f, 0.6f);
		c[ImGuiCol_NavWindowingDimBg] = G(0.0f, 0.5f);
		c[ImGuiCol_ModalWindowDimBg] = G(0.0f, 0.55f);
#ifdef IMGUI_HAS_DOCK
		c[ImGuiCol_DockingPreview] = A(accent, 0.40f);
		c[ImGuiCol_DockingEmptyBg] = bg0;
#endif
	}

	inline bool CloseButton(const char* id) {
		const float size = ImGui::GetFrameHeight();
		ImVec2 p = ImGui::GetCursorScreenPos();
		bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
		const bool hovered = ImGui::IsItemHovered();
		const bool held = ImGui::IsItemActive();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		if (hovered || held) {
			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size),
				held ? IM_COL32(179, 46, 61, 255) : IM_COL32(230, 77, 89, 217), ImGui::GetStyle().FrameRounding);
		}

		ImVec2 c(p.x + size * 0.5f, p.y + size * 0.5f);
		const float r = 4.0f;
		ImU32 col = hovered || held ? IM_COL32(255, 255, 255, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
		dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 1.6f);
		dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, 1.6f);

		return pressed;
	}
}