#pragma once
#include <string>
#include <glm/glm.hpp>
#include "../Component.h"   
#include "../../Core/GUIEngine.h"

class TextComponent : public ComponentBase<TextComponent>, public IUIDrawable {
public:
	enum class HAlign { Left = 0, Center, Right };
	enum class VAlign { Top = 0, Middle, Bottom };

	std::string text = "New Text";
	std::string fontPath;
	glm::vec4   color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float       fontSize = 24.0f;      
	HAlign      hAlign = HAlign::Left;
	VAlign      vAlign = VAlign::Top;
	bool        wordWrap = true;

	TextComponent(Object* parent);

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
};