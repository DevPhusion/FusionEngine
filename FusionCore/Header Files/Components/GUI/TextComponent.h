#pragma once
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include "../Component.h"   
#include "../../Core/GUIEngine.h"
#include "../../Core/RichText.h"

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
	void ParseRich();

	std::vector<RichText::Piece> richCache;
	std::string richCacheSrc;
	bool richCacheRich = false;
	bool richCacheValid = false;

	static int SelectionCallback(ImGuiInputTextCallbackData* data);
	int selStart = 0;
	int selEnd = 0;
};