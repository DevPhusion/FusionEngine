#pragma once
#include <glm/glm.hpp>
#include "../Component.h"
#include "../../Core/GUIEngine.h"

class PanelComponent : public ComponentBase<PanelComponent>, public IUIDrawable {
public:
	glm::vec4 color{ 0.2f, 0.2f, 0.2f, 1.0f };
	float     cornerRadius = 0.0f;       
	glm::vec4 borderColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	float     borderThickness = 0.0f;    

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
};
