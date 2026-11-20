#include "../../Header Files/Core/GUIEngine.h"
#include "../../Header Files/Core/EngineManager.h"
#include "../../Header Files/Core/Editor/Windows/Console.h"
#include "../../Header Files/Components/GUI/CanvasComponent.h"
#include "../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../Header Files/Core/Files/Export/ExportPackageReader.h"
#include "../../Header Files/Core/ObjectManager.h"
#include "../../Header Files/Core/Editor/GUIGizmos.h"
#include <algorithm>
#include <cfloat>
#include <cstdint>


void GUIEngine::Initialize(GLFWwindow* window) {
	if (initialized) return;
	initialized = true;

	if (ImGui::GetCurrentContext()) return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
	ownsContext = true;
}

void GUIEngine::Shutdown() {
	for (auto& [path, entry] : textureCache) glDeleteTextures(1, &entry.first);
	textureCache.clear();
	for (auto& pending : pendingTextureDeletes) glDeleteTextures(1, &pending.id);
	pendingTextureDeletes.clear();

	if (ownsContext) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		ownsContext = false;
	}
	initialized = false;
	drawables.clear();
}

void GUIEngine::RenderPlayer(int vpX, int vpY, int vpW, int vpH, int windowW, int windowH) {
	if (!ownsContext || vpW <= 0 || vpH <= 0) return;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	const float topY = (float)(windowH - (vpY + vpH));
	RenderCanvases(ImGui::GetBackgroundDrawList(),
		glm::vec2((float)vpX, topY), glm::vec2((float)vpW, (float)vpH));

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUIEngine::RenderInEditor(ImDrawList* dl, glm::vec2 origin, glm::vec2 size) {
	if (!dl) return;
	dl->PushClipRect(ImVec2(origin.x, origin.y), ImVec2(origin.x + size.x, origin.y + size.y), true);
	RenderCanvases(dl, origin, size);
	GUIGizmos::getInstance().Draw(dl, origin, size);
	dl->PopClipRect();
}

bool GUIEngine::HasDrawable(Object* owner) const {
	auto it = drawables.find(owner);
	return it != drawables.end() && !it->second.empty();
}

ImFont* GUIEngine::GetFont(const std::string& virtualPath) {
	if (virtualPath.empty() || !ImGui::GetCurrentContext()) return nullptr;

	auto it = fontCache.find(virtualPath);
	if (it != fontCache.end()) return it->second;

	ImFont* font = LoadFont(virtualPath);
	fontCache[virtualPath] = font;
	return font;
}

ImFont* GUIEngine::GetFontOrDefault(const std::string& virtualPath) {
	ImFont* font = GetFont(virtualPath);
	return font ? font : ImGui::GetFont();
}

ImFont* GUIEngine::LoadFont(const std::string& virtualPath) {
	std::vector<uint8_t> bytes;

	std::error_code ec;
	const std::filesystem::path abs = FileManager::getInstance().VirtualToAbsolute(virtualPath);
	if (std::filesystem::exists(abs, ec) && !ec) {
		std::ifstream in(abs, std::ios::binary | std::ios::ate);
		if (in.is_open()) {
			const std::streamsize size = in.tellg();
			if (size > 0) {
				bytes.resize((size_t)size);
				in.seekg(0);
				in.read(reinterpret_cast<char*>(bytes.data()), size);
			}
		}
	}
	else if (const std::vector<uint8_t>* packed = ExportPackageReader::getInstance().Get(virtualPath)) {
		bytes = *packed;
	}

	if (bytes.empty()) return nullptr;

	void* mem = IM_ALLOC(bytes.size());
	std::memcpy(mem, bytes.data(), bytes.size());

	ImFontConfig cfg;
	const std::string name = std::filesystem::path(virtualPath).filename().string();
	strncpy_s(cfg.Name, sizeof(cfg.Name), name.c_str(), _TRUNCATE);

	return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(mem, (int)bytes.size(), 0.0f, &cfg);
}

unsigned int GUIEngine::AcquireTexture(const std::string& path) {
	auto it = textureCache.find(path);
	if (it != textureCache.end()) {
		it->second.second++;
		return it->second.first;
	}

	int width = 0, height = 0, channels = 0;
	stbi_set_flip_vertically_on_load(true);   
	unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);   
	if (!data) {
		Console::PrintError("Failed to load GUI texture: " + path);
		return 0;
	}

	GLint prevBinding = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevBinding);

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, (GLuint)prevBinding);
	stbi_image_free(data);

	textureCache[path] = { id, 1 };
	return id;
}

void GUIEngine::ReleaseTexture(const std::string& path) {
	auto it = textureCache.find(path);
	if (it == textureCache.end()) return;
	if (--it->second.second <= 0) {
		QueueTextureDelete(it->second.first);
		textureCache.erase(it);
	}
}

void GUIEngine::QueueTextureDelete(unsigned int id) {
	if (id == 0) return;
	if (!ImGui::GetCurrentContext()) { glDeleteTextures(1, &id); return; }
	pendingTextureDeletes.push_back({ id, ImGui::GetFrameCount() });
}

void GUIEngine::FlushTextureDeletes() {
	if (pendingTextureDeletes.empty() || !ImGui::GetCurrentContext()) return;
	const int now = ImGui::GetFrameCount();
	for (size_t i = 0; i < pendingTextureDeletes.size();) {
		if (pendingTextureDeletes[i].frame < now) {
			glDeleteTextures(1, &pendingTextureDeletes[i].id);
			pendingTextureDeletes.erase(pendingTextureDeletes.begin() + i);
		}
		else ++i;
	}
}

unsigned int GUITextureHandle::Resolve(const std::string& path) {
	if (EngineManager::getInstance().isHeadless) return 0;   
	if (path == loadedPath) return id;                      

	Release();
	loadedPath = path;
	if (!path.empty()) id = GUIEngine::getInstance().AcquireTexture(path);
	return id;
}

void GUITextureHandle::Release() {
	if (id != 0) GUIEngine::getInstance().ReleaseTexture(loadedPath);
	id = 0;
	loadedPath.clear();
}

bool GUIEngine::InputEnabled() {
	EngineManager& engine = EngineManager::getInstance();
	return engine.isPlayer || engine.EnginePhysicsMode == EngineManager::PhysicsMode::Simulate;
}

ImU32 GUIEngine::ToU32(const glm::vec4& c, float alphaMul) {
	return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a * alphaMul));
}

float GUIEngine::ScaledRadius(const UIDrawContext& c, float radiusUnits) {
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float limit = std::min(mx.x - mn.x, mx.y - mn.y) * 0.5f;
	return std::max(0.0f, std::min(std::max(radiusUnits, 0.0f) * c.scale, limit));
}

void GUIEngine::DrawFill(const UIDrawContext& c, const glm::vec4& color, float radiusPx) {
	if (color.a <= 0.0f) return;
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	c.dl->AddRectFilled(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), ToU32(color), radiusPx);
}

void GUIEngine::DrawTexture(const UIDrawContext& c, unsigned int tex, const glm::vec4& tint, float radiusPx) {
	if (tex == 0 || tint.a <= 0.0f) return;
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	c.dl->AddImageRounded((ImTextureID)(intptr_t)tex, ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y),
		ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), ToU32(tint), radiusPx);
}

void GUIEngine::DrawBorder(const UIDrawContext& c, const glm::vec4& color, float thicknessUnits, float radiusPx) {
	if (thicknessUnits <= 0.0f || color.a <= 0.0f) return;
	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float t = thicknessUnits * c.scale;
	const float inset = t * 0.5f;
	if (mx.x - mn.x <= t || mx.y - mn.y <= t) return;
	c.dl->AddRect(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset),
		ToU32(color), std::max(radiusPx - inset, 0.0f), 0, t);
}

void GUIEngine::DrawCenteredText(const UIDrawContext& c, const std::string& text, ImFont* font,
	float sizeUnits, const glm::vec4& color) {
	if (text.empty() || sizeUnits <= 0.0f || color.a <= 0.0f) return;
	if (!font) font = ImGui::GetFont();

	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float px = sizeUnits * c.scale;
	const ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text.c_str());
	const ImVec2 pos(mn.x + ((mx.x - mn.x) - ts.x) * 0.5f, mn.y + ((mx.y - mn.y) - ts.y) * 0.5f);

	c.dl->PushClipRect(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), true);
	c.dl->AddText(font, px, pos, ToU32(color), text.c_str());
	c.dl->PopClipRect();
}

void GUIEngine::RegisterDrawable(Object* owner, IUIDrawable* d) {
	auto& v = drawables[owner];
	if (std::find(v.begin(), v.end(), d) == v.end()) v.push_back(d);
}

void GUIEngine::UnregisterDrawable(Object* owner, IUIDrawable* d) {
	auto it = drawables.find(owner);
	if (it == drawables.end()) return;
	auto& v = it->second;
	v.erase(std::remove(v.begin(), v.end(), d), v.end());
	if (v.empty()) drawables.erase(it);
}

void GUIEngine::RenderCanvases(ImDrawList* dl, glm::vec2 origin, glm::vec2 size) {
	FlushTextureDeletes();

	if (size.x <= 0.0f || size.y <= 0.0f) return;

	std::vector<CanvasComponent*> canvases = CanvasComponent::GetActiveCanvases();   

	for (CanvasComponent* canvas : canvases) {
		if (canvas->renderMode != CanvasComponent::RenderMode::ScreenSpace) continue; 

		canvas->UpdateCanvasSize((int)size.x, (int)size.y);

		Object* canvasObj = canvas->parent;
		if (!canvasObj) continue;

		if (canvas->IsLayoutDirty()) {
			const UiRect root{ glm::vec2(0.0f), canvas->GetCanvasSize() };
			for (Object* child : canvasObj->children) {
				if (RectTransformComponent* rt = child->GetComponent<RectTransformComponent>())
					rt->ResolveTree(root);
			}
			canvas->ClearLayoutDirty();
		}

		UIDrawContext ctx;
		ctx.dl = dl;
		ctx.origin = origin;
		ctx.scale = canvas->GetScaleFactor();
		DrawTree(canvasObj, ctx);
	}
}

void GUIEngine::DrawTree(Object* node, UIDrawContext& ctx) {
	for (Object* child : node->children) {
		RectTransformComponent* rt = child->GetComponent<RectTransformComponent>();
		if (!rt || child->hidden) continue;
		ctx.rect = rt->GetRect();

		auto it = drawables.find(child);
		if (it != drawables.end()) {
			for (IUIDrawable* d : it->second) {
				if (d->IsUIVisible()) d->DrawUI(ctx);
			}
		}
		DrawTree(child, ctx);
	}
}