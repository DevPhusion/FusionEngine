#include "../../../Header Files/Components/GUI/PanelComponent.h"
#include "../../../Header Files/Components/GUI/RectTransformComponent.h"
#include "../../../Header Files/Core/Editor/EditorField.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
	EditorField::FileSpec MakeTextureSpec() {
		EditorField::FileSpec s = EditorField::FileSpec::ImageFile();   // same spec RenderComponent uses
		s.allowClear = true;
		s.previewHeight = 96.0f;
		return s;
	}

	std::unordered_map<std::string, std::pair<unsigned int, int>>& TextureCache() {
		static std::unordered_map<std::string, std::pair<unsigned int, int>> cache;
		return cache;
	}

	struct PendingDelete { unsigned int id; int frame; };
	std::vector<PendingDelete>& PendingDeletes() {
		static std::vector<PendingDelete> v;
		return v;
	}

	void QueueTextureDelete(unsigned int id) {
		if (id == 0) return;
		if (!ImGui::GetCurrentContext()) { glDeleteTextures(1, &id); return; }
		PendingDeletes().push_back({ id, ImGui::GetFrameCount() });
	}

	void FlushTextureDeletes() {
		auto& v = PendingDeletes();
		if (v.empty() || !ImGui::GetCurrentContext()) return;
		const int now = ImGui::GetFrameCount();
		for (size_t i = 0; i < v.size();) {
			if (v[i].frame < now) {
				glDeleteTextures(1, &v[i].id);
				v.erase(v.begin() + i);
			}
			else ++i;
		}
	}

	unsigned int AcquireTexture(const std::string& path) {
		auto& cache = TextureCache();
		auto it = cache.find(path);
		if (it != cache.end()) {
			it->second.second++;
			return it->second.first;
		}

		int width = 0, height = 0, channels = 0;
		stbi_set_flip_vertically_on_load(true);   
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);  
		if (!data) {
			Console::PrintError("Failed to load panel texture: " + path);
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

		cache[path] = { id, 1 };
		return id;
	}

	void ReleaseCachedTexture(const std::string& path) {
		auto& cache = TextureCache();
		auto it = cache.find(path);
		if (it == cache.end()) return;
		if (--it->second.second <= 0) {
			QueueTextureDelete(it->second.first);
			cache.erase(it);
		}
	}
}

PanelComponent::PanelComponent(Object* parent) : ComponentBase<PanelComponent>(parent) {
	Name = "Panel Component";
}

void PanelComponent::Activate() {
	Component::Activate();

	if (!parent->GetComponent<RectTransformComponent>())
		parent->AddComponent(std::make_unique<RectTransformComponent>(parent));

	if (Enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
}

void PanelComponent::Deactivate() {
	Component::Deactivate();
	GUIEngine::getInstance().UnregisterDrawable(parent, this);
}

void PanelComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (!isActive) return;
	if (enabled) GUIEngine::getInstance().RegisterDrawable(parent, this);
	else         GUIEngine::getInstance().UnregisterDrawable(parent, this);
}

void PanelComponent::OnDelete() {
	Deactivate();
	if (!EngineManager::getInstance().isHeadless) ReleaseTexture();
}

void PanelComponent::ReleaseTexture() {
	if (textureID != 0 && !loadedPath.empty())
		ReleaseCachedTexture(loadedPath);
	textureID = 0;
	loadedPath.clear();
}

void PanelComponent::EnsureTexture() {
	if (EngineManager::getInstance().isHeadless) return;   
	if (loadedPath == texturePath) return;                 

	ReleaseTexture();
	loadedPath = texturePath;
	if (texturePath.empty()) return;

	textureID = AcquireTexture(texturePath);
}

void PanelComponent::DrawUI(const UIDrawContext& c) {
	FlushTextureDeletes();

	const glm::vec2 mn = c.ScreenMin();
	const glm::vec2 mx = c.ScreenMax();
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	if (w <= 0.0f || h <= 0.0f) return;

	const float radius = std::min(std::max(cornerRadius, 0.0f) * c.scale, std::min(w, h) * 0.5f);

	if (mode == PanelMode::Texture) {
		EnsureTexture();
		if (textureID != 0 && tint.a > 0.0f) {
			const ImU32 tcol = ImGui::ColorConvertFloat4ToU32(ImVec4(tint.r, tint.g, tint.b, tint.a));
			c.dl->AddImageRounded((ImTextureID)(intptr_t)textureID, ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y),
				ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), tcol, radius);
		}
	}
	else if (color.a > 0.0f) {
		const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
		c.dl->AddRectFilled(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y), col, radius);
	}

	if (borderThickness > 0.0f && borderColor.a > 0.0f) {
		const float t = borderThickness * c.scale;
		const float inset = t * 0.5f;
		if (w > t && h > t) {
			const ImU32 bcol = ImGui::ColorConvertFloat4ToU32(
				ImVec4(borderColor.r, borderColor.g, borderColor.b, borderColor.a));
			c.dl->AddRect(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset),
				bcol, std::max(radius - inset, 0.0f), 0, t);
		}
	}
}

void PanelComponent::ProcessInspectorUI() {
	static const char* modeNames[] = { "Color", "Texture" };
	static const EditorField::FileSpec textureSpec = MakeTextureSpec();

	int pm = (int)mode;
	EditorField::ComboScene(parent, "Mode ", "##PanelMode", &pm, modeNames, 2, [&] {
		mode = (PanelMode)pm;
		});

	if (mode == PanelMode::Color) {
		float col[4] = { color.r, color.g, color.b, color.a };
		EditorField::ColorEdit4Scene(parent, "Color ", "##PanelColor", col, [&] {
			color = glm::vec4(col[0], col[1], col[2], col[3]);
			});
	}
	else {
		EditorField::FileScene(parent, "Texture ", "##PanelTexture", texturePath, [&] {}, textureSpec);

		EnsureTexture();   
		if (texturePath.empty())
			ImGui::TextDisabled("No texture selected.");
		else if (textureID == 0)
			ImGui::TextDisabled("Could not load texture.");

		float tcol[4] = { tint.r, tint.g, tint.b, tint.a };
		EditorField::ColorEdit4Scene(parent, "Tint ", "##PanelTint", tcol, [&] {
			tint = glm::vec4(tcol[0], tcol[1], tcol[2], tcol[3]);
			});
	}

	float cr = cornerRadius;
	EditorField::InputFloatScene(parent, "Corner Radius ", "##PanelRadius", &cr, [&] {
		cornerRadius = std::max(cr, 0.0f);
		}, "%.1f");

	ImGui::Separator();

	float bt = borderThickness;
	EditorField::InputFloatScene(parent, "Border Thickness ", "##PanelBorderThickness", &bt, [&] {
		borderThickness = std::max(bt, 0.0f);
		}, "%.1f");

	float bcol[4] = { borderColor.r, borderColor.g, borderColor.b, borderColor.a };
	EditorField::ColorEdit4Scene(parent, "Border Color ", "##PanelBorderColor", bcol, [&] {
		borderColor = glm::vec4(bcol[0], bcol[1], bcol[2], bcol[3]);
		});
}

void PanelComponent::CopyTo(Object* other) {
	PanelComponent* target = other->GetComponent<PanelComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<PanelComponent>(other));
		target = other->GetComponent<PanelComponent>();
	}
	target->SetEnabled(Enabled);
	target->mode = mode;
	target->color = color;
	target->texturePath = texturePath;   
	target->tint = tint;
	target->cornerRadius = cornerRadius;
	target->borderColor = borderColor;
	target->borderThickness = borderThickness;
}

void PanelComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(color);
	w.Write(cornerRadius);
	w.Write(borderColor);
	w.Write(borderThickness);
	w.Write((int)mode);
	w.WriteString(texturePath);
	w.Write(tint);
}

void PanelComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	color = r.Read<glm::vec4>();
	cornerRadius = r.Read<float>();
	borderColor = r.Read<glm::vec4>();
	borderThickness = r.Read<float>();
	mode = (PanelMode)r.Read<int>();
	texturePath = r.ReadString();   
	tint = r.Read<glm::vec4>();
}