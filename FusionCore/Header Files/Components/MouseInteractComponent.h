#pragma once
#include "Component.h"
#include "../Core/InputManager.h"
#include "../Objects/Object.h"
#include "SoftBodyComponent.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "EditorRenderComponent.h"
#include "../Core/EngineManager.h"
#include "VertexComponent.h"
#include "../Core/Physics/Forces/MouseDrag.h"
#include "../Core/Physics/PhysicsEngine.h"

class MouseInteractComponent : public ComponentBase<MouseInteractComponent>
{
public:
	MouseInteractComponent(Object* parent, bool physicsInteract);
	MouseInteractComponent() = default;

	static bool ObjectSelected; //prevent multiple selection;
	bool physicsInteract;
	bool Selected;
	bool Inspectable = true;

	MouseDrag* mouseDragForce = nullptr;


	virtual void ProcessInspectorUI();
	virtual void OnDelete();
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	void FindSelectedPolygon(int button, int action, int mods);
	void DragPolygon(double xpos, double ypos);
	void SetSelectedPolygon(Object* obj, bool enable);
	void OnPhysicsModeChanged();
private:
	std::vector<int> mouseButtonCallbackID;
	std::vector<int> cursorPosCallbackID;
	int physicsModeChangedCallbackID;
};

