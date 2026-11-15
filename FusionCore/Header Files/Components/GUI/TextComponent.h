#pragma once
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include "../Component.h"   
#include "../../Core/GUIEngine.h"

class TextComponent : public ComponentBase<TextComponent>, public IUIDrawable {
public:
	enum class HAlign { Left = 0, Center, Right };
	enum class VAlign { Top = 0, Middle, Bottom };
	enum class TextMode { Plain = 0, Rich = 1 };

	TextComponent(Object* parent);

	std::string text = "New Text";
	std::string fontPath;
	std::string boldFontPath;
	std::string italicFontPath;
	std::string boldItalicFontPath;
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float fontSize = 24.0f;      
	HAlign hAlign = HAlign::Left;
	VAlign vAlign = VAlign::Top;
	TextMode textMode = TextMode::Plain;
	bool wordWrap = true;

	bool IsRich() const { return textMode == TextMode::Rich; }

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
	struct RichPiece {
		std::string text;
		bool bold = false, italic = false, underline = false, strike = false;
		bool hasColor = false;          // false -> use the component's `color`
		glm::vec4 color{ 1.0f };
		float size = 0.0f;              // 0 -> use the component's `fontSize`
		bool newline = false;
		bool space = false;
	};

	void ParseRich();

	std::vector<RichPiece> richCache;
	std::string richCacheSrc;
	bool richCacheRich = false;
	bool richCacheValid = false;
};