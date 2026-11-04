#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include <glm/glm.hpp>
#include <vector>

class CanvasComponent : public ComponentBase<CanvasComponent>
{
public:
	enum class RenderMode : int {
		ScreenSpace = 0,   
		WorldSpace = 1   
	};

	enum class ScaleMode : int {
		ConstantPixelSize = 0,  
		ScaleWithScreen = 1    
	};

	CanvasComponent(Object* parent);
	CanvasComponent() = default;

	virtual void Activate();
	virtual void Deactivate();
	virtual void SetEnabled(bool enabled);
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	void UpdateCanvasSize(int viewportWidthPx, int viewportHeightPx);

	glm::vec2 GetCanvasSize() const { return canvasSize; }
	float GetScaleFactor() const { return scaleFactor; }

	glm::mat4 GetProjection() const;

	glm::vec2 ViewportToCanvas(glm::vec2 uv01) const { return uv01 * canvasSize; }

	void MarkLayoutDirty() { layoutDirty = true; }
	bool IsLayoutDirty() const { return layoutDirty; }
	void ClearLayoutDirty() { layoutDirty = false; }

	static const std::vector<CanvasComponent*>& GetActiveCanvases() { return activeCanvases; }

	RenderMode renderMode = RenderMode::ScreenSpace;
	ScaleMode scaleMode = ScaleMode::ScaleWithScreen;
	int sortOrder = 0;
	glm::vec2 referenceResolution = glm::vec2(1920.0f, 1080.0f);
	float matchWidthOrHeight = 0.5f;   
	float constantScale = 1.0f;

private:
	glm::vec2 canvasSize = glm::vec2(1920.0f, 1080.0f);
	float scaleFactor = 1.0f;
	bool layoutDirty = true;

	static std::vector<CanvasComponent*> activeCanvases;
	void RegisterCanvas();
	void UnregisterCanvas();
	static void SortCanvases();
};