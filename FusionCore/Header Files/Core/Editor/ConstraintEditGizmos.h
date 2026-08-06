#pragma once
#include "../../Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include <vector>

class ConstraintEditGizmos
{
public:
	ConstraintEditGizmos();
	~ConstraintEditGizmos() = default;

	void RegisterConstraint(Constraint* c);
	void UnregisterConstraint(Constraint* c);

	void DrawConstraintDisplays();

	void DrawPivotHandles();

	void OnMouseButton(int button, int action, int mods);
	void OnCursorPosition(double xpos, double ypos);

	std::vector<Constraint*> registeredConstraints;

private:
	enum class DragTarget { None, AttachA, AttachB };
	Constraint* dragConstraint = nullptr;
	DragTarget dragTarget = DragTarget::None;

	glm::vec3 currentMouseWorld = glm::vec3(0.0f);

	std::vector<int> mouseButtonCallbackID;
	std::vector<int> cursorPosCallbackID;

	const float handleScreenSize = 0.02f;
	const float hitPadding = 0.03f;
	const glm::vec4 handleColorA = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
	const glm::vec4 handleColorB = glm::vec4(0.2f, 0.4f, 1.0f, 1.0f);
	const glm::vec4 draggedHandleColor = glm::vec4(1.0f, 0.6f, 0.1f, 1.0f);

	bool HitTestHandle(glm::vec3 worldPos, glm::vec3 handleWorldPos);
	void DrawHandle(glm::vec3 worldPos, const glm::vec4& color);
};