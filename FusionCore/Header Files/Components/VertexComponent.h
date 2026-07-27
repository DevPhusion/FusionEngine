#pragma once
#include "Component.h"
#include "../Objects/VertexPoint.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "../Core/Physics/PhysicsEngine.h"
#include "../Core/InputManager.h"
#include "../Core/EngineManager.h"
#include <memory>

class VertexComponent : public ComponentBase<VertexComponent>
{
public:
	VertexComponent(Object* parent);
	VertexComponent() = default;

	static bool vertexSelected;

	std::vector<VertexPoint*> vertexPoints;
	int selectedIndex = -1;

	std::vector<int> mouseButtonCallbackID;
	std::vector<int> cursorPosCallbackID;

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	int GetSelectedVertex();
	void RemoveAllVertex();
	void SetEnabled(bool enabled) override;
	void SetVertexPoints(std::vector<VertexPoint*> vertexPoints);
	void FindSelectedPoint(int button, int action, int mods);
	void DragPoint(double xpos, double ypos);
	void UpdateTransform();
};

