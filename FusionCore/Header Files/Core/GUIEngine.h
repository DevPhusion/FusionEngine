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
	glm::vec2 origin = glm::vec2(0.0f);   
	float scale = 1.0f;     
	UiRect rect;             

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

class GUITextureHandle {
public:
	GUITextureHandle() = default;
	GUITextureHandle(const GUITextureHandle&) {}
	GUITextureHandle& operator=(const GUITextureHandle&) { return *this; }

	unsigned int Resolve(const std::string& path);   
	void         Release();
	unsigned int Id() const { return id; }

private:
	std::string  loadedPath;
	unsigned int id = 0;
};

class GUIEngine {
public:
	static GUIEngine& getInstance() {
		static GUIEngine instance;
		return instance;
	}

	void Initialize(GLFWwindow* window);
	void Shutdown();

	static bool InputEnabled();

	static ImU32 ToU32(const glm::vec4& c, float alphaMul = 1.0f);
	static float ScaledRadius(const UIDrawContext& c, float radiusUnits);   
	static void  DrawFill(const UIDrawContext& c, const glm::vec4& color, float radiusPx);
	static void  DrawTexture(const UIDrawContext& c, unsigned int tex, const glm::vec4& tint, float radiusPx);
	static void  DrawBorder(const UIDrawContext& c, const glm::vec4& color, float thicknessUnits, float radiusPx);
	static void  DrawCenteredText(const UIDrawContext& c, const std::string& text, ImFont* font,
		float sizeUnits, const glm::vec4& color);

	void RenderPlayer(int vpX, int vpY, int vpW, int vpH, int windowW, int windowH);

	void RenderInEditor(ImDrawList* dl, glm::vec2 origin, glm::vec2 size);

	bool HasDrawable(Object* owner) const;

	void RegisterDrawable(Object* owner, IUIDrawable* d);
	void UnregisterDrawable(Object* owner, IUIDrawable* d);

	ImFont* GetFontOrDefault(const std::string& virtualPath);
	ImFont* GetFont(const std::string& virtualPath);

	unsigned int AcquireTexture(const std::string& path);  
	void ReleaseTexture(const std::string& path);

private:
	GUIEngine() = default;

	void RenderCanvases(ImDrawList* dl, glm::vec2 origin, glm::vec2 size);
	void DrawTree(Object* node, UIDrawContext& ctx);

	ImFont* LoadFont(const std::string& virtualPath);

	std::unordered_map<std::string, std::pair<unsigned int, int>> textureCache;
	struct PendingTextureDelete { unsigned int id; int frame; };
	std::vector<PendingTextureDelete> pendingTextureDeletes;
	void QueueTextureDelete(unsigned int id);
	void FlushTextureDeletes();

	std::unordered_map<std::string, ImFont*> fontCache;
	bool ownsContext = false;
	bool initialized = false;
	std::unordered_map<Object*, std::vector<IUIDrawable*>> drawables;
};