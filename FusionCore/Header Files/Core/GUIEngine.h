#pragma once
#include "../Components/GUI/RectTransformComponent.h" 
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <algorithm>

class CanvasComponent;

struct UIDrawContext {
	ImDrawList* dl = nullptr;
	glm::vec2   origin{ 0.0f };   
	float       scale = 1.0f;     
	UiRect      rect;             

	glm::vec2 ToScreen(glm::vec2 canvasPos) const { return origin + canvasPos * scale; }
	glm::vec2 ScreenMin() const { return ToScreen(rect.pos); }
	glm::vec2 ScreenMax() const { return ToScreen(rect.pos + rect.size); }
};

class IUIDrawable {
public:
	virtual ~IUIDrawable() = default;
	virtual void DrawUI(const UIDrawContext& ctx) = 0;
	virtual bool IsUIVisible() const { return true; }
};

class GUIEngine {
public:
	static GUIEngine& getInstance() {
		static GUIEngine instance;
		return instance;
	}

	void Initialize(GLFWwindow* window);
	void Shutdown();

	void RenderPlayer(int vpX, int vpY, int vpW, int vpH, int windowW, int windowH);

	void RenderInEditor(ImDrawList* dl, glm::vec2 origin, glm::vec2 size);

	bool HasDrawable(Object* owner) const;

	ImFont* GetFont(const std::string& virtualPath);

	void RegisterDrawable(Object* owner, IUIDrawable* d);
	void UnregisterDrawable(Object* owner, IUIDrawable* d);

private:
	GUIEngine() = default;

	void RenderCanvases(ImDrawList* dl, glm::vec2 origin, glm::vec2 size);
	void DrawTree(Object* node, UIDrawContext& ctx);

	ImFont* LoadFont(const std::string& virtualPath);

	std::unordered_map<std::string, ImFont*> fontCache;
	bool ownsContext = false;
	bool initialized = false;
	std::unordered_map<Object*, std::vector<IUIDrawable*>> drawables;
};