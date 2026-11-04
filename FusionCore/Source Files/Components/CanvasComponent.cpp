#include "../../Header Files/Components/CanvasComponent.h"
#include "../../Header Files/Core/EngineManager.h"
#include "../../Header Files/Core/Editor/EditorField.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui/imgui.h>

std::vector<CanvasComponent*> CanvasComponent::activeCanvases;

CanvasComponent::CanvasComponent(Object* parent) : ComponentBase<CanvasComponent>(parent) {
	Name = "Canvas Component";
}

void CanvasComponent::RegisterCanvas() {
	if (std::find(activeCanvases.begin(), activeCanvases.end(), this) != activeCanvases.end()) return;
	activeCanvases.push_back(this);
	SortCanvases();
	layoutDirty = true;
}

void CanvasComponent::UnregisterCanvas() {
	activeCanvases.erase(std::remove(activeCanvases.begin(), activeCanvases.end(), this), activeCanvases.end());
}

void CanvasComponent::SortCanvases() {
	std::stable_sort(activeCanvases.begin(), activeCanvases.end(),
		[](CanvasComponent* a, CanvasComponent* b) { return a->sortOrder < b->sortOrder; });
}

void CanvasComponent::Activate() {
	Component::Activate();
	if (!Enabled) return;
	RegisterCanvas();
}

void CanvasComponent::Deactivate() {
	Component::Deactivate();
	UnregisterCanvas();
}

void CanvasComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (!isActive) return;

	if (enabled) RegisterCanvas();
	else UnregisterCanvas();
}

void CanvasComponent::OnDelete() {
	Deactivate();
}

void CanvasComponent::UpdateCanvasSize(int viewportWidthPx, int viewportHeightPx) {
	if (viewportWidthPx <= 0 || viewportHeightPx <= 0) return;

	float newScale = 1.0f;
	if (scaleMode == ScaleMode::ScaleWithScreen) {
		float refW = std::max(referenceResolution.x, 1.0f);
		float refH = std::max(referenceResolution.y, 1.0f);
		float logW = std::log2((float)viewportWidthPx / refW);
		float logH = std::log2((float)viewportHeightPx / refH);
		newScale = std::exp2(glm::mix(logW, logH, glm::clamp(matchWidthOrHeight, 0.0f, 1.0f)));
	}
	else {
		newScale = constantScale;
	}
	newScale = std::max(newScale, 0.0001f);

	glm::vec2 newSize((float)viewportWidthPx / newScale, (float)viewportHeightPx / newScale);

	if (newScale != scaleFactor || newSize != canvasSize) {
		scaleFactor = newScale;
		canvasSize = newSize;
		layoutDirty = true;
	}
}

glm::mat4 CanvasComponent::GetProjection() const {
	return glm::ortho(0.0f, canvasSize.x, canvasSize.y, 0.0f, -1.0f, 1.0f);
}

void CanvasComponent::ProcessInspectorUI() {
	static const char* renderModeNames[] = { "Screen Space", "World Space" };
	static const char* scaleModeNames[] = { "Constant Pixel Size", "Scale With Screen" };

	int rm = (int)renderMode;
	EditorField::ComboScene(parent, "Render Mode ", "##RenderMode", &rm, renderModeNames, 2, [&] {
		renderMode = (RenderMode)rm;
		layoutDirty = true;
		});
	if (renderMode == RenderMode::WorldSpace)
		ImGui::TextDisabled("World space is not implemented yet.");

	int order = sortOrder;
	EditorField::InputIntScene(parent, "Sort Order ", "##SortOrder", &order, [&] {
		sortOrder = order;
		SortCanvases();
		});

	ImGui::Separator();

	int sm = (int)scaleMode;
	EditorField::ComboScene(parent, "Scale Mode ", "##ScaleMode", &sm, scaleModeNames, 2, [&] {
		scaleMode = (ScaleMode)sm;
		layoutDirty = true;
		});

	if (scaleMode == ScaleMode::ScaleWithScreen) {
		int ref[2] = { (int)referenceResolution.x, (int)referenceResolution.y };
		EditorField::InputInt2Scene(parent, "Reference Resolution ", "##RefRes", ref, [&] {
			referenceResolution = glm::vec2((float)std::max(ref[0], 1), (float)std::max(ref[1], 1));
			layoutDirty = true;
			});

		float match = matchWidthOrHeight;
		EditorField::Wrap(parent, "Match Width/Height ",
			[&] { return ImGui::SliderFloat("##Match", &match, 0.0f, 1.0f, "%.2f"); },
			[&] { matchWidthOrHeight = match; layoutDirty = true; });
	}
	else {
		float cs = constantScale;
		EditorField::InputFloatScene(parent, "Constant Scale ", "##ConstScale", &cs, [&] {
			constantScale = std::max(cs, 0.01f);
			layoutDirty = true;
			});
	}

	ImGui::Separator();

	if (EditorField::BeginPropertyTable("##CanvasInfo")) {
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.0f x %.0f", canvasSize.x, canvasSize.y);
		EditorField::PropertyRow("Canvas Size", buf);
		std::snprintf(buf, sizeof(buf), "%.3f", scaleFactor);
		EditorField::PropertyRow("Scale Factor", buf);
		EditorField::EndPropertyTable();
	}
}

void CanvasComponent::CopyTo(Object* other) {
	CanvasComponent* target = other->GetComponent<CanvasComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<CanvasComponent>(other));
		target = other->GetComponent<CanvasComponent>();
	}
	target->SetEnabled(Enabled);
	target->renderMode = renderMode;
	target->scaleMode = scaleMode;
	target->sortOrder = sortOrder;
	target->referenceResolution = referenceResolution;
	target->matchWidthOrHeight = matchWidthOrHeight;
	target->constantScale = constantScale;
	target->layoutDirty = true;
}

void CanvasComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write((int)renderMode);
	w.Write((int)scaleMode);
	w.Write(sortOrder);
	w.Write(referenceResolution.x);
	w.Write(referenceResolution.y);
	w.Write(matchWidthOrHeight);
	w.Write(constantScale);
}

void CanvasComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	renderMode = (RenderMode)r.Read<int>();
	scaleMode = (ScaleMode)r.Read<int>();
	sortOrder = r.Read<int>();
	referenceResolution.x = r.Read<float>();
	referenceResolution.y = r.Read<float>();
	matchWidthOrHeight = r.Read<float>();
	constantScale = r.Read<float>();
	layoutDirty = true;
}