#include "../../Header Files/Core/GUIEngine.h"
#include "../../Header Files/Components/GUI/CanvasComponent.h"
#include "../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../Header Files/Core/Files/Export/ExportPackageReader.h"
#include "../../Header Files/Core/ObjectManager.h"


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
	dl->PopClipRect();
}

ImFont* GUIEngine::GetFont(const std::string& virtualPath) {
	if (virtualPath.empty() || !ImGui::GetCurrentContext()) return nullptr;

	auto it = fontCache.find(virtualPath);
	if (it != fontCache.end()) return it->second;   

	ImFont* font = LoadFont(virtualPath);
	fontCache[virtualPath] = font;
	return font;
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
	if (size.x <= 0.0f || size.y <= 0.0f) return;

	std::vector<CanvasComponent*> canvases = CanvasComponent::GetActiveCanvases();   // sorted by sortOrder

	for (CanvasComponent* canvas : canvases) {
		if (canvas->renderMode != CanvasComponent::RenderMode::ScreenSpace) continue; // world space TODO

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