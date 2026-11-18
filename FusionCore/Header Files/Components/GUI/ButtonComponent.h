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
		Click = 0,      
		Press,          
		Release,        
		HoverEnter,    
		HoverExit      
	};
	using Callback = std::function<void()>;

	std::string text = "Button";
	std::string fontPath;                              
	float       fontSize = 18.0f;
	glm::vec4   textColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	glm::vec4 normalColor{ 0.20f, 0.20f, 0.20f, 1.0f };     
	glm::vec4 hoverColor{ 0.26f, 0.26f, 0.26f, 1.0f };      
	glm::vec4 pressedColor{ 0.15f, 0.15f, 0.15f, 1.0f };    
	glm::vec4 disabledColor{ 0.20f, 0.20f, 0.20f, 0.50f };
	float     cornerRadius = 6.0f;
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
	void UpdateInput(const glm::vec2& mn, const glm::vec2& mx);
	void Fire(ButtonEvent event);

	static constexpr int kEventCount = 5;
	std::unordered_map<int, Callback> callbacks[kEventCount];
	int nextCallbackId = 1;

	bool hovered = false;
	bool pressed = false;
	int  lastInputFrame = -1;   
};