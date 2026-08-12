#include "../../../../Header Files/Core/Editor/Windows/Viewport.h"
#include "../../../../Header Files/Core/EngineManager.h"
#include "../../../../imgui/imgui.h"

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
	ImGuiWindowClass statusWindowClass;
	statusWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
	ImGui::SetNextWindowClass(&statusWindowClass);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(name.c_str());

	isHovered = ImGui::IsWindowHovered();
	isFocused = ImGui::IsWindowFocused();

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
	}
	else {
		panelSize = ImVec2(0, 0);               
	}

	ImGui::End();
	ImGui::PopStyleVar();
}