#pragma once
#include "../Component.h"
#include "../../Objects/Object.h"
#include <glm/glm.hpp>

class CanvasComponent;

struct UiRect {
	glm::vec2 pos = glm::vec2(0.0f);
	glm::vec2 size = glm::vec2(0.0f);

	glm::vec2 Max() const { return pos + size; }
	glm::vec2 Center() const { return pos + size * 0.5f; }

	bool Contains(glm::vec2 p) const {
		return p.x >= pos.x && p.y >= pos.y && p.x < pos.x + size.x && p.y < pos.y + size.y;
	}

	UiRect Intersect(const UiRect& o) const {
		glm::vec2 mn = glm::max(pos, o.pos);
		glm::vec2 mx = glm::min(Max(), o.Max());
		return UiRect{ mn, glm::max(mx - mn, glm::vec2(0.0f)) };
	}
};

class RectTransformComponent : public ComponentBase<RectTransformComponent>
{
public:
	RectTransformComponent(Object* parent);
	RectTransformComponent() = default;

	virtual void Activate();
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	glm::vec2 anchorMin = glm::vec2(0.5f);
	glm::vec2 anchorMax = glm::vec2(0.5f);
	glm::vec2 offsetMin = glm::vec2(-50.0f);
	glm::vec2 offsetMax = glm::vec2(50.0f);
	glm::vec2 minSize = glm::vec2(0.0f);

	void SetAnchors(glm::vec2 anchorMin, glm::vec2 anchorMax);
	void SetOffsets(glm::vec2 offsetMin, glm::vec2 offsetMax);
	void SetMinSize(glm::vec2 minSize);
	void SetAnchoredRect(glm::vec2 centerFromAnchor, glm::vec2 size);

	bool IsStretched() const { return anchorMin != anchorMax; }
	const UiRect& GetRect() const { return rect; }

	void Resolve(const UiRect& parentRect);
	void ResolveTree(const UiRect& parentRect);

	UiRect GetParentRect() const;
	CanvasComponent* FindCanvas() const;
	void MarkDirty();
private:
	UiRect rect;   
};