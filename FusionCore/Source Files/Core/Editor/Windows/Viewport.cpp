#include "../../../../Header Files/Core/Editor/Windows/Viewport.h"
#include "../../../../Header Files/Core/EngineManager.h"
#include "../../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../../imgui/imgui.h"

namespace {
	enum class ToolIcon { Move, Rotate, Scale };

	void DrawArrowHead(ImDrawList* dl, ImVec2 tip, ImVec2 dir, float len, ImU32 col) {
		ImVec2 perp(-dir.y, dir.x);
		ImVec2 base(tip.x - dir.x * len, tip.y - dir.y * len);
		float w = len * 0.6f;
		dl->AddTriangleFilled(tip,
			ImVec2(base.x + perp.x * w, base.y + perp.y * w),
			ImVec2(base.x - perp.x * w, base.y - perp.y * w), col);
	}

	void DrawToolIcon(ImDrawList* dl, ImVec2 c, float r, ToolIcon icon, ImU32 col) {
		const float th = 1.6f;
		switch (icon) {
		case ToolIcon::Move: {
			const ImVec2 dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
			for (const ImVec2& d : dirs) {
				ImVec2 tip(c.x + d.x * r, c.y + d.y * r);
				dl->AddLine(c, ImVec2(tip.x - d.x * 3.0f, tip.y - d.y * 3.0f), col, th);
				DrawArrowHead(dl, tip, d, 5.0f, col);
			}
			break;
		}
		case ToolIcon::Rotate: {
			const float rad = r * 0.85f;
			const float a0 = 0.5f, a1 = a0 + 1.6f * 3.14159265f;
			dl->PathArcTo(c, rad, a0, a1, 28);
			dl->PathStroke(col, ImDrawFlags_None, th);
			ImVec2 end(c.x + cosf(a1) * rad, c.y + sinf(a1) * rad);
			ImVec2 tangent(-sinf(a1), cosf(a1));
			DrawArrowHead(dl, ImVec2(end.x + tangent.x * 3.5f, end.y + tangent.y * 3.5f), tangent, 5.5f, col);
			break;
		}
		case ToolIcon::Scale: {
			const float h = r * 0.8f;
			ImVec2 mn(c.x - h, c.y - h), mx(c.x + h, c.y + h);
			dl->AddRect(mn, mx, col, 1.5f, 0, th);
			dl->AddRectFilled(ImVec2(mn.x, c.y), ImVec2(c.x, mx.y), col);
			ImVec2 dir(0.7071f, -0.7071f);
			dl->AddLine(c, ImVec2(mx.x - 2.0f, mn.y + 2.0f), col, th);
			DrawArrowHead(dl, ImVec2(mx.x + 1.0f, mn.y - 1.0f), dir, 5.0f, col);
			break;
		}
		}
	}

	bool ToolButton(const char* id, ToolIcon icon, bool active, const char* tooltip, float size) {
		ImVec2 p = ImGui::GetCursorScreenPos();
		bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
		const bool hovered = ImGui::IsItemHovered();
		const bool held = ImGui::IsItemActive();

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec4 accent = EditorTheme::Accent();
		const float rounding = ImGui::GetStyle().FrameRounding;

		if (active) {
			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size),
				ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.22f)), rounding);
		}
		else if (hovered || held) {
			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size),
				ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered), rounding);
		}

		ImU32 col = active ? ImGui::GetColorU32(accent) : ImGui::GetColorU32(ImGuiCol_Text);
		DrawToolIcon(dl, ImVec2(p.x + size * 0.5f, p.y + size * 0.5f), size * 0.30f, icon, col);

		if (hovered) ImGui::SetTooltip("%s", tooltip);
		return pressed;
	}

	bool DrawGizmoToolbar(ImVec2 origin) {
		Gizmos* gizmos = Renderer::getInstance().gizmos;
		if (!gizmos) return false;

		const float btn = 28.0f, pad = 4.0f, gap = 2.0f;
		const ImVec2 size(btn + pad * 2.0f, btn * 3.0f + gap * 2.0f + pad * 2.0f);

		ImGui::SetCursorScreenPos(origin);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, gap));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 0.88f));

		bool hovered = false;
		if (ImGui::BeginChild("##GizmoToolbar", size, ImGuiChildFlags_Borders,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			hovered = ImGui::IsWindowHovered();

			auto tool = [&](const char* id, ToolIcon icon, GizmosMode mode, const char* tip) {
				if (ToolButton(id, icon, gizmos->currentGizmosMode == mode, tip, btn))
					gizmos->SwitchMode(mode);
				};
			tool("##move", ToolIcon::Move, GizmosMode::Move, "Move");
			tool("##rotate", ToolIcon::Rotate, GizmosMode::Rotate, "Rotate");
			tool("##scale", ToolIcon::Scale, GizmosMode::Scale, "Scale");
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		return hovered;
	}
}

Viewport::Viewport(std::string name) : EditorWindow(name) {
	CreateFramebuffer(EngineManager::getInstance().resolutionWidth,
		EngineManager::getInstance().resolutionHeight);
}

Viewport::~Viewport() {
	DestroyFramebuffer();
}

void Viewport::CreateFramebuffer(int width, int height) {
	textureWidth = width;
	textureHeight = height;

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glGenTextures(1, &colorTexture);
	glBindTexture(GL_TEXTURE_2D, colorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

	glGenRenderbuffers(1, &depthStencilRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		Console::PrintError("Viewport: Incomplete frame buffer");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewport::DestroyFramebuffer() {
	if (colorTexture)     glDeleteTextures(1, &colorTexture);
	if (depthStencilRBO)  glDeleteRenderbuffers(1, &depthStencilRBO);
	if (fbo)              glDeleteFramebuffers(1, &fbo);
	colorTexture = depthStencilRBO = fbo = 0;
}

void Viewport::BeginRenderGame() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, textureWidth, textureHeight);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Viewport::EndRenderGame() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewport::Resize(int width, int height) {
	if (width == textureWidth && height == textureHeight) return;
	if (width <= 0 || height <= 0) return;

	DestroyFramebuffer();
	CreateFramebuffer(width, height);
}

void Viewport::ProcessWindow() {
	EditorTheme::ApplyDockClass(ImGuiDockNodeFlags_NoTabBar);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(name.c_str());

	isHovered = ImGui::IsWindowHovered();
	isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	overlayHovered = false;

	ImVec2 avail = ImGui::GetContentRegionAvail();
	if (avail.x > 0 && avail.y > 0) {
		float gameAspect = (float)textureWidth / (float)textureHeight;
		float panelAspect = avail.x / avail.y;

		ImVec2 imageSize;
		if (panelAspect > gameAspect) {
			imageSize.y = avail.y;
			imageSize.x = avail.y * gameAspect;
		}
		else {
			imageSize.x = avail.x;
			imageSize.y = avail.x / gameAspect;
		}

		ImVec2 cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(
			cursor.x + (avail.x - imageSize.x) * 0.5f,
			cursor.y + (avail.y - imageSize.y) * 0.5f
		));

		panelPos = ImGui::GetCursorScreenPos(); 
		panelSize = imageSize;               

		ImGui::Image((ImTextureID)(intptr_t)colorTexture, imageSize, ImVec2(0, 1), ImVec2(1, 0));
		const bool showOverlay =
			EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate;

		overlayHovered = showOverlay
			? DrawGizmoToolbar(ImVec2(panelPos.x + 10.0f, panelPos.y + 10.0f))
			: false;
	}
	else {
		panelSize = ImVec2(0, 0);               
	}

	ImGui::End();
	ImGui::PopStyleVar();
}