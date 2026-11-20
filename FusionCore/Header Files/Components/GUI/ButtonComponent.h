#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "../Component.h"
#include "../../Core/GUIEngine.h"

class ButtonComponent : public ComponentBase<ButtonComponent>, public IUIDrawable {
public:
	enum class ButtonEvent {
		Click = 0,       // mouse pressed AND released while over the button
		Press,           // mouse went down over the button
		Release,         // mouse released after a Press (inside or outside the button)
		HoverEnter,      // pointer moved onto the button
		HoverExit        // pointer moved off the button
	};
	using Callback = std::function<void()>;

	enum class ButtonMode { Color = 0, Texture = 1 };

	ButtonMode mode = ButtonMode::Color;

	std::string text = "Button";
	std::string fontPath;                             
	float       fontSize = 18.0f;
	glm::vec4   textColor{ 0.90f, 0.90f, 0.90f, 1.0f };     

	// Color mode

	glm::vec4 normalColor{ 0.20f, 0.20f, 0.20f, 1.0f };      
	glm::vec4 hoverColor{ 0.26f, 0.26f, 0.26f, 1.0f };       
	glm::vec4 pressedColor{ 0.15f, 0.15f, 0.15f, 1.0f };     
	glm::vec4 disabledColor{ 0.20f, 0.20f, 0.20f, 0.50f };  

	// Texture mode

	std::string texturePath;
	std::string hoverTexturePath;
	std::string pressedTexturePath;
	std::string disabledTexturePath;
	
	glm::vec4 normalTint{ 1.00f, 1.00f, 1.00f, 1.00f };
	glm::vec4 hoverTint{ 0.85f, 0.85f, 0.85f, 1.00f };
	glm::vec4 pressedTint{ 0.70f, 0.70f, 0.70f, 1.00f };
	glm::vec4 disabledTint{ 1.00f, 1.00f, 1.00f, 0.50f };

	float     cornerRadius = 4.0f;
	glm::vec4 borderColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	float     borderThickness = 0.0f;

	bool interactable = true;

	ButtonComponent(Object* parent);

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

	int  AddCallback(ButtonEvent event, Callback cb);
	void RemoveCallback(int id);     
	void ClearCallbacks();            

	void Click();

	bool IsHovered() const { return hovered; }
	bool IsPressed() const { return pressed; }

private:
	enum State { Normal = 0, Hover, Pressed, Disabled, StateCount };

	void UpdateInput(const glm::vec2& mn, const glm::vec2& mx);
	void Fire(ButtonEvent event);
	const std::string& TexturePathFor(int state) const;
	const glm::vec4& TintFor(int state) const;

	GUITextureHandle textures[StateCount];

	static constexpr int kEventCount = 5;
	std::unordered_map<int, Callback> callbacks[kEventCount];
	int nextCallbackId = 1;

	bool hovered = false;
	bool pressed = false;
	int  lastInputFrame = -1;   
};