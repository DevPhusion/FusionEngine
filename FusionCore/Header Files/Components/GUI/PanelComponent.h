#pragma once
#include <string>
#include <glm/glm.hpp>
#include "../Component.h"
#include "../../Core/GUIEngine.h"

class PanelComponent : public ComponentBase<PanelComponent>, public IUIDrawable {
public:
	enum class PanelMode { Color = 0, Texture = 1 };

	PanelMode mode = PanelMode::Color;

	glm::vec4 color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);   // Color mode
	std::string texturePath;                        // Texture mode 
	glm::vec4 tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);    // Texture mode modulate

	float cornerRadius = 0.0f;
	glm::vec4 borderColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	float borderThickness = 0.0f;

	PanelComponent(Object* parent);

	void Activate() override;
	void Deactivate() override;
	void SetEnabled(bool enabled) override;
	void OnDelete() override;

	void DrawUI(const UIDrawContext& ctx) override;
	bool IsUIVisible() const override { return Enabled; }

	void ProcessInspectorUI() override;
	void CopyTo(Object* other) override;
	void Serialize(BinaryWriter& w) override;
	void Deserialize(BinaryReader& r) override;

private:
	GUITextureHandle texture;
};