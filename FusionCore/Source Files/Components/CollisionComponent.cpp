#include "../../Header Files/Components/CollisionComponent.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"
#include "../../Header Files/Core/ObjectManager.h"
#include <glm/gtc/constants.hpp>
#include <array>
#include <algorithm>

CollisionComponent::CollisionComponent(Object* parent) : ComponentBase<CollisionComponent>(parent) {
	Name = "Collision Component";

	int firstId = AddShape(PolygonShape{}, "Shape 1");
	resolutionShapeID = firstId;

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc) {
		CollisionShapeEntry* entry = GetShape(firstId);
		if (entry) SetSyncWithRenderComponent(*entry, true);
	}
}

void CollisionComponent::Activate() {
	isActive = true;

	if (parent->HasComponent<FluidComponent>()) return;

	onTransformCallbackID = parent->GetComponent<TransformComponent>()->AddTransformCallback([this]() {
		this->calculateBoundingCircle();
		});

	physicsChangeEventCallbackID = EngineManager::getInstance().AddPhysicsModeChangedEvent([this]() {
		if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
			bool anyEditing = false;
			for (auto& entry : shapes) {
				if (entry.polygonEditCallbackID != -1) {
					Renderer::getInstance().polygonEditGizmos->EndEdit();
					Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
					entry.polygonEditCallbackID = -1;
					entry.isAddVertex = false;
					anyEditing = true;
				}
			}
			if (anyEditing) {
				EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
			}
		}
		});

	if (Enabled) SetEnabled(true);   
}

void CollisionComponent::Deactivate() {
	if (!isActive) return;

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc && onTransformCallbackID != -1) {
		tc->RemoveTransformCallback(onTransformCallbackID);
		onTransformCallbackID = -1;
	}

	if (physicsChangeEventCallbackID != -1) {
		EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsChangeEventCallbackID);
		physicsChangeEventCallbackID = -1;
	}

	for (auto& entry : shapes) {
		if (entry.polygonEditCallbackID != -1) {
			Renderer::getInstance().polygonEditGizmos->EndEdit();
			Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
			entry.polygonEditCallbackID = -1;
			entry.isAddVertex = false;
		}
		if (entry.BAHnode) {
			PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
			entry.BAHnode = nullptr;
		}
	}

	isActive = false;
}

int CollisionComponent::AddShape(Shape shape, std::string name) {
	CollisionShapeEntry entry;
	entry.id = nextShapeID++;
	entry.name = name.empty() ? ("Shape " + std::to_string(entry.id + 1)) : name;
	entry.currentShape = shape;
	RebuildFromShape(entry, VerticesFromShape(shape));

	shapes.push_back(std::move(entry));

	if (resolutionShapeID == -1) {
		resolutionShapeID = shapes.back().id;
	}

	SyncResolutionShapeFields();
	calculateBoundingCircle(shapes.back());
	EngineManager::getInstance().SceneChangeEvent();
	return shapes.back().id;
}

void CollisionComponent::RemoveShape(int shapeId) {
	auto it = std::find_if(shapes.begin(), shapes.end(), [&](CollisionShapeEntry& e) { return e.id == shapeId; });
	if (it == shapes.end()) return;

	if (it->renderSyncCallbackID != -1) {
		RenderComponent* rc = parent->GetComponent<RenderComponent>();
		if (rc) rc->RemoveOnShapeSetCallback(it->renderSyncCallbackID);
	}
	if (it->polygonEditCallbackID != -1) {
		Renderer::getInstance().polygonEditGizmos->EndEdit();
		Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(it->polygonEditCallbackID);
	}
	if (it->BAHnode) {
		PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, it->id);
		it->BAHnode = nullptr;
	}

	bool wasResolution = (it->id == resolutionShapeID);
	shapes.erase(it);

	if (wasResolution) {
		resolutionShapeID = shapes.empty() ? -1 : shapes.front().id;
	}

	SyncResolutionShapeFields();
	calculateBoundingCircle();
	EngineManager::getInstance().SceneChangeEvent();
}

CollisionShapeEntry* CollisionComponent::GetShape(int shapeId) {
	for (auto& e : shapes) if (e.id == shapeId) return &e;
	return nullptr;
}

CollisionShapeEntry* CollisionComponent::GetResolutionShape() {
	return GetShape(resolutionShapeID);
}

void CollisionComponent::SetResolutionShapeID(int shapeId) {
	resolutionShapeID = shapeId;
	SyncResolutionShapeFields();
	calculateBoundingCircle();
	EngineManager::getInstance().SceneChangeEvent();
}

void CollisionComponent::SyncResolutionShapeFields() {
	CollisionShapeEntry* res = GetResolutionShape();
	if (res) {
		resolutionShape = res->currentShape;
		points = res->points;
		edges = res->edges;
	}
	else {
		resolutionShape = PolygonShape{};
		points.clear();
		edges.clear();
	}
}

std::vector<glm::vec3> CollisionComponent::VerticesFromShape(Shape& shape) {
	TransformComponent* tc = parent->HasComponent<TransformComponent>()
		? parent->GetComponent<TransformComponent>() : nullptr;

	return std::visit([&](auto&& s) -> std::vector<glm::vec3> {
		using T = std::decay_t<decltype(s)>;

		if constexpr (std::is_same_v<T, PolygonShape>) {
			std::vector<glm::vec3> verts;
			for (size_t i = 0; i + 1 < s.vertices.size(); i += 5) {
				verts.push_back(glm::vec3(s.vertices[i], s.vertices[i + 1], 0.0f));
			}
			return verts;
		}
		else if constexpr (std::is_same_v<T, RectangleShape>) {
			float hw = s.width * 0.5f;
			float hh = s.height * 0.5f;
			std::array<glm::vec3, 4> worldCorners = {
				s.center + glm::vec3(-hw, -hh, 0.0f),
				s.center + glm::vec3(hw, -hh, 0.0f),
				s.center + glm::vec3(hw,  hh, 0.0f),
				s.center + glm::vec3(-hw,  hh, 0.0f),
			};

			std::vector<glm::vec3> verts;
			for (int i = 0; i < 4; i++) {
				verts.push_back(tc ? tc->ProjectToWorld(worldCorners[i], true) : worldCorners[i]);
			}
			return verts;
		}
		else {
			std::vector<glm::vec3> verts;
			int ps = s.segments;
			for (int i = 0; i < ps; i++) {
				float theta = 2.0f * glm::pi<float>() * float(i) / float(ps);
				glm::vec3 worldPoint = s.center + glm::vec3(s.radius * std::cos(theta), s.radius * std::sin(theta), 0.0f);
				verts.push_back(tc ? tc->ProjectToWorld(worldPoint, true) : worldPoint);
			}
			return verts;
		}
		}, shape);
}

void CollisionComponent::RebuildFromShape(CollisionShapeEntry& entry, const std::vector<glm::vec3>& localVerts) {
	entry.points.clear();
	entry.points.reserve(localVerts.size());
	for (int i = 0; i < (int)localVerts.size(); i++) {
		entry.points.push_back({ localVerts[i].x, localVerts[i].y, float(i) });
	}

	entry.edges.clear();
	if (entry.points.size() >= 2) {
		entry.edges.reserve(entry.points.size());
		for (int i = 0; i < (int)entry.points.size(); i++) {
			Edge edge;
			edge.start = glm::vec3(entry.points[i][0], entry.points[i][1], 0);
			edge.end = glm::vec3(entry.points[(i + 1) % entry.points.size()][0], entry.points[(i + 1) % entry.points.size()][1], 0);
			entry.edges.push_back(edge);
		}
	}
}

void CollisionComponent::SetShape(CollisionShapeEntry& entry, Shape shape) {
	entry.currentShape = shape;
	RebuildFromShape(entry, VerticesFromShape(shape));
	SyncResolutionShapeFields();
	calculateBoundingCircle();
	EngineManager::getInstance().SceneChangeEvent();
}

void CollisionComponent::SetShape(Shape shape) {
	CollisionShapeEntry* res = GetResolutionShape();
	if (!res && !shapes.empty()) res = &shapes.front();
	if (!res) {
		int newId = AddShape(shape);
		resolutionShapeID = newId;
		SyncResolutionShapeFields();
		return;
	}
	SetShape(*res, shape);
}

void CollisionComponent::ApplyLiveShapeUpdate(CollisionShapeEntry& entry, const std::vector<glm::vec3>& verts) {
	if (verts.size() < 3) return;
	std::vector<float> newVertices;
	newVertices.reserve(verts.size() * 5);

	glm::vec3 bmin(INFINITY), bmax(-INFINITY);
	for (auto& v : verts) { bmin = glm::min(bmin, v); bmax = glm::max(bmax, v); }
	glm::vec3 range = glm::max(bmax - bmin, glm::vec3(1e-6f));

	for (auto& v : verts) {
		float u = (v.x - bmin.x) / range.x;
		float uvY = (v.y - bmin.y) / range.y;
		newVertices.insert(newVertices.end(), { v.x, v.y, 0.0f, u, uvY });
	}

	PolygonShape s;
	s.vertices = newVertices;
	SetShape(entry, s);
}

glm::vec3 CollisionComponent::GetCenter(CollisionShapeEntry& entry) {
	float A = 0, C_x = 0, C_y = 0;
	int n = (int)entry.points.size();
	if (n < 3) return glm::vec3(0);
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		float shoelace = entry.points[i][0] * entry.points[j][1] - entry.points[j][0] * entry.points[i][1];
		A += shoelace;
		C_x += (entry.points[i][0] + entry.points[j][0]) * shoelace;
		C_y += (entry.points[i][1] + entry.points[j][1]) * shoelace;
	}
	A *= 0.5f;
	if (A == 0) return glm::vec3(0);
	C_x /= (6.0f * A);
	C_y /= (6.0f * A);
	return glm::vec3(C_x, C_y, 0);
}

float CollisionComponent::GetArea(CollisionShapeEntry& entry) {
	int n = (int)entry.points.size();
	if (n < 3) return 0.0f;
	float area = 0.0f;
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		area += entry.points[i][0] * entry.points[j][1];
		area -= entry.points[j][0] * entry.points[i][1];
	}
	return std::abs(area) * 0.5f;
}

glm::vec3 CollisionComponent::GetCenter() {
	CollisionShapeEntry* res = GetResolutionShape();
	if (!res && !shapes.empty()) res = &shapes.front();
	return res ? GetCenter(*res) : glm::vec3(0);
}

float CollisionComponent::GetArea() {
	CollisionShapeEntry* res = GetResolutionShape();
	if (!res && !shapes.empty()) res = &shapes.front();
	return res ? GetArea(*res) : 0.0f;
}

int CollisionComponent::GetShapeId(const std::string& name) {
	for (auto& e : shapes) {
		if (e.name == name) return e.id;
	}
	return -1;
}

void CollisionComponent::Draw() {
	if (!Enabled) return;

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;

	for (auto& entry : shapes) {
		if (entry.points.size() < 3) continue;

		std::vector<glm::vec3> worldPoints;
		worldPoints.reserve(entry.points.size());
		for (auto& p : entry.points) {
			worldPoints.push_back(tc->ProjectToWorld(glm::vec3(p[0], p[1], 0.0f)));
		}

		bool isResolution = (entry.id == resolutionShapeID);
		glm::vec4 fillColor = isResolution ? glm::vec4(0.3f, 0.7f, 1.0f, 0.25f) : glm::vec4(1.0f, 0.8f, 0.2f, 0.15f);
		glm::vec4 outlineColor = isResolution ? glm::vec4(0.3f, 0.7f, 1.0f, 0.85f) : glm::vec4(1.0f, 0.8f, 0.2f, 0.7f);

		Renderer::getInstance().DrawFilledPolygon(worldPoints, fillColor, outlineColor, 1.5f);
	}
}

void CollisionComponent::SetSyncWithRenderComponent(CollisionShapeEntry& entry, bool sync) {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (!rc) {
		entry.syncWithRenderComponent = false;
		return;
	}

	if (sync) {
		entry.syncWithRenderComponent = true;
		if (entry.renderSyncCallbackID == -1) {
			int id = entry.id;
			entry.renderSyncCallbackID = rc->AddOnShapeSetCallback([this, id]() {
				CollisionShapeEntry* e = GetShape(id);
				if (e) this->SyncFromRenderComponent(*e);
				});
		}
		SyncFromRenderComponent(entry);
	}
	else {
		if (entry.renderSyncCallbackID != -1) {
			rc->RemoveOnShapeSetCallback(entry.renderSyncCallbackID);
			entry.renderSyncCallbackID = -1;
		}
		entry.syncWithRenderComponent = false;
	}
}

void CollisionComponent::SyncFromRenderComponent(CollisionShapeEntry& entry) {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (!rc) return;

	entry.currentShape = rc->currentShape;

	entry.points.clear();
	entry.points.reserve(rc->edges.size());
	for (size_t i = 0; i < rc->edges.size(); i++) {
		entry.points.push_back({ rc->edges[i].start.x, rc->edges[i].start.y, float(i) });
	}
	entry.edges = rc->edges;

	SyncResolutionShapeFields();
	calculateBoundingCircle();
	EngineManager::getInstance().SceneChangeEvent();
}

void CollisionComponent::SetCollisionLayer(uint16_t layer) {
	collisionLayer = layer;

	for (auto& entry : shapes) {
		entry.boundingCircle.collisionLayer = collisionLayer;
		if (entry.BAHnode) entry.BAHnode->area.collisionLayer = collisionLayer;
	}

	FluidComponent* fc = parent->GetComponent<FluidComponent>();
	if (fc) fc->UpdateCollisionLayerMask();

	EngineManager::getInstance().SceneChangeEvent();
}

void CollisionComponent::SetCollisionMask(uint16_t mask) {
	collisionMask = mask;

	for (auto& entry : shapes) {
		entry.boundingCircle.collisionMask = collisionMask;
		if (entry.BAHnode) entry.BAHnode->area.collisionMask = collisionMask;
	}

	FluidComponent* fc = parent->GetComponent<FluidComponent>();
	if (fc) fc->UpdateCollisionLayerMask();

	EngineManager::getInstance().SceneChangeEvent();
}

bool CollisionComponent::isGrounded(float probeLength) {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc || points.empty()) return false;

	glm::vec3 worldLowest;
	float lowestWorldY = INFINITY;
	for (auto& p : points) {
		glm::vec3 worldP = tc->ProjectToWorld(glm::vec3(p[0], p[1], 0.0f));
		if (worldP.y < lowestWorldY) {
			lowestWorldY = worldP.y;
			worldLowest = worldP;
		}
	}

	const float skin = 0.05f;
	glm::vec3 origin = worldLowest + glm::vec3(0.0f, skin, 0.0f);
	origin.x = tc->worldPosition.x;

	RayCastHit hit = PhysicsEngine::getInstance().RayCast(
		origin, glm::vec3(0.0f, -1.0f, 0.0f), probeLength + skin,
		collisionMask, { parent });

	return hit.hit;
}

int CollisionComponent::AddCollisionCallback(std::function<void(const CollisionEventData&)> callback) {
	int id = nextCollisionCallbackID++;
	collisionCallbacks[id] = std::move(callback);
	return id;
}

void CollisionComponent::RemoveCollisionCallback(int id) {
	collisionCallbacks.erase(id);
}

void CollisionComponent::NotifyCollision(const CollisionEventData& data) {
	if (!Enabled) return;
	for (auto& [id, cb] : collisionCallbacks) {
		cb(data);
	}
}

int CollisionComponent::AddCollisionEnterCallback(std::function<void(const CollisionEventData&)> callback) {
	int id = nextCollisionEnterCallbackID++;
	collisionEnterCallbacks[id] = std::move(callback);
	return id;
}
void CollisionComponent::RemoveCollisionEnterCallback(int id) { collisionEnterCallbacks.erase(id); }
void CollisionComponent::NotifyCollisionEnter(const CollisionEventData& data) {
	if (!Enabled) return;
	for (auto& [id, cb] : collisionEnterCallbacks) cb(data);
}

int CollisionComponent::AddCollisionExitCallback(std::function<void(const CollisionEventData&)> callback) {
	int id = nextCollisionExitCallbackID++;
	collisionExitCallbacks[id] = std::move(callback);
	return id;
}

void CollisionComponent::RemoveCollisionExitCallback(int id)
{
	collisionExitCallbacks.erase(id);
}

void CollisionComponent::NotifyCollisionExit(const CollisionEventData& data) {
	for (auto& [id, cb] : collisionExitCallbacks) cb(data);
}

void CollisionComponent::DrawLayerMaskUI(const char* label, uint16_t* layer) {
	if (!layer) return;

	ImGui::Text("%s", label);
	ImGui::BeginGroup();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

	ImVec2 startPos = ImGui::GetCursorPos();

	const float btnSize = 24.0f;
	const float gap = 2.0f;

	for (int i = 0; i < 16; ++i) {
		int col = i % 8;
		int row = i / 8;
		ImGui::SetCursorPos(ImVec2(
			startPos.x + col * (btnSize + gap),
			startPos.y + row * (btnSize + gap)
		));

		bool is_set = (*layer & (1 << i)) != 0;
		if (is_set)
			ImGui::PushStyleColor(ImGuiCol_Button,
				ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

		ImGui::PushID(label);
		ImGui::PushID(i);

		if (ImGui::Button(std::to_string(i + 1).c_str(), ImVec2(btnSize, btnSize))) {
			EditorManager::getInstance().BeginEdit({ parent });
			*layer ^= (1 << i);
			if (layer == &collisionLayer) {
				SetCollisionLayer(collisionLayer);
			}
			else {
				SetCollisionMask(collisionMask);
			}
			EditorManager::getInstance().EndEdit({ parent });
		}

		ImGui::PopID();
		ImGui::PopID();

		if (is_set)
			ImGui::PopStyleColor();
	}

	ImGui::SetCursorPos(ImVec2(startPos.x, startPos.y + 2 * (btnSize + gap)));
	ImGui::Dummy(ImVec2(8 * (btnSize + gap) - gap, 0));

	ImGui::PopStyleVar(2);
	ImGui::EndGroup();
}

void CollisionComponent::DrawShapePreview(CollisionShapeEntry& entry) {
	ImGui::Spacing();
	ImGui::Text("Preview");

	const ImVec2 previewSize(100.0f, 100.0f);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImU32 outlineCol = IM_COL32(80, 200, 120, 220);

	draw->AddRectFilled(pos, ImVec2(pos.x + previewSize.x, pos.y + previewSize.y), IM_COL32(40, 40, 40, 255), 4.0f);

	const float cx = pos.x + previewSize.x * 0.5f;
	const float cy = pos.y + previewSize.y * 0.5f;

	std::vector<std::pair<float, float>> displayVerts;
	for (auto& p : entry.points) displayVerts.push_back({ p[0], p[1] });

	if (displayVerts.size() >= 3) {
		float minX = displayVerts[0].first, maxX = minX;
		float minY = displayVerts[0].second, maxY = minY;
		for (auto& [x, y] : displayVerts) {
			minX = std::min(minX, x); maxX = std::max(maxX, x);
			minY = std::min(minY, y); maxY = std::max(maxY, y);
		}

		float scx = (minX + maxX) * 0.5f;
		float scy = (minY + maxY) * 0.5f;
		float range = std::max(maxX - minX, maxY - minY);
		float scale = (range > 0.f) ? (previewSize.x * 0.76f / range) : 1.f;

		ImVector<ImVec2> pts;
		for (auto& [x, y] : displayVerts)
			pts.push_back({ cx + (x - scx) * scale, cy - (y - scy) * scale });

		draw->AddPolyline(pts.Data, pts.Size, outlineCol, ImDrawFlags_Closed, 1.5f);
	}
	else {
		draw->AddText({ pos.x + 4, pos.y + 4 }, IM_COL32(200, 200, 200, 180), "No shape");
	}

	ImGui::Dummy(previewSize);
}

void CollisionComponent::ProcessShapeEntryUI(CollisionShapeEntry& entry) {
	char nameBuf[64];
#if defined(_MSC_VER)
	strcpy_s(nameBuf, entry.name.c_str());
#else
	strncpy(nameBuf, entry.name.c_str(), sizeof(nameBuf) - 1);
	nameBuf[sizeof(nameBuf) - 1] = '\0';
#endif
	ImGui::Text("Name");
	ImGui::SameLine();
	if (ImGui::InputText("##ShapeName", nameBuf, IM_ARRAYSIZE(nameBuf))) {
		entry.name = nameBuf;
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc) {
		bool sync = entry.syncWithRenderComponent;
		if (ImGui::Checkbox("Sync with Render Component", &sync)) {
			EditorManager::getInstance().BeginEdit({ parent });
			SetSyncWithRenderComponent(entry, sync);
			EditorManager::getInstance().EndEdit({ parent });
		}
		ImGui::Separator();
	}

	if (entry.syncWithRenderComponent) {
		ImGui::BeginDisabled();
	}

	ImGui::Text("Shape");
	ImGui::SameLine();

	const char* shapeLabel = std::visit([](auto&& s) -> const char* {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape>) return "Rectangle";
		else if constexpr (std::is_same_v<T, CircleShape>)  return "Circle";
		else                                                  return "Polygon";
		}, entry.currentShape);

	if (ImGui::BeginCombo("##CollisionShapeSelect", shapeLabel)) {
		if (ImGui::Selectable("Rectangle", std::holds_alternative<RectangleShape>(entry.currentShape))) {
			EditorManager::getInstance().BeginEdit({ parent });

			RectangleShape rect;
			rect.width = rect.height = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			rect.center = tc ? tc->GetWorldPosition() : GetCenter(entry);

			if (entry.polygonEditCallbackID != -1) {
				Renderer::getInstance().polygonEditGizmos->EndEdit();
				Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
				entry.polygonEditCallbackID = -1;
			}
			entry.isAddVertex = false;
			SetShape(entry, rect);

			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("Circle", std::holds_alternative<CircleShape>(entry.currentShape))) {
			EditorManager::getInstance().BeginEdit({ parent });

			CircleShape cir;
			cir.radius = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			cir.center = tc ? tc->GetWorldPosition() : GetCenter(entry);

			if (entry.polygonEditCallbackID != -1) {
				Renderer::getInstance().polygonEditGizmos->EndEdit();
				Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
				entry.polygonEditCallbackID = -1;
			}
			entry.isAddVertex = false;
			SetShape(entry, cir);

			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("Polygon", std::holds_alternative<PolygonShape>(entry.currentShape))) {
			EditorManager::getInstance().BeginEdit({ parent });   // closed by the vertex-edit session below, not here

			PolygonShape poly;
			poly.vertices = {};
			SetShape(entry, poly);

			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			Renderer::getInstance().polygonEditGizmos->BeginEdit(tc);

			EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
			entry.isAddVertex = true;
		}
		ImGui::EndCombo();
	}

	std::visit([this, &entry](auto&& s) {
		using T = std::decay_t<decltype(s)>;

		if constexpr (std::is_same_v<T, RectangleShape>) {
			float dims[2] = { s.width, s.height };
			ImGui::Text("  Size");
			ImGui::SameLine();
			if (ImGui::InputFloat2("##CollisionRectSize", dims, "%.3f m")) {
				s.width = std::max(0.01f, dims[0]);
				s.height = std::max(0.01f, dims[1]);
				TransformComponent* tc = parent->GetComponent<TransformComponent>();
				s.center = tc ? tc->GetWorldPosition() : GetCenter(entry);
				SetShape(entry, s);
			}
			if (ImGui::IsItemActivated()) {
				EditorManager::getInstance().BeginEdit({ parent });
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				EditorManager::getInstance().EndEdit({ parent });
			}
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			auto updateCenter = [&]() {
				TransformComponent* tc = parent->GetComponent<TransformComponent>();
				s.center = tc ? tc->GetWorldPosition() : GetCenter(entry);
				};

			float r = s.radius;
			ImGui::Text("  Radius");
			ImGui::SameLine();
			if (ImGui::InputFloat("##CollisionCircleRadius", &r, 0.0f, 0.0f, "%.3f m")) {
				s.radius = std::max(0.01f, r); updateCenter(); SetShape(entry, s);
			}
			if (ImGui::IsItemActivated()) {
				EditorManager::getInstance().BeginEdit({ parent });
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				EditorManager::getInstance().EndEdit({ parent });
			}

			int seg = s.segments;
			ImGui::Text("  Segments");
			ImGui::SameLine();
			if (ImGui::InputInt("##CollisionCircleSeg", &seg)) {
				s.segments = std::max(3, seg); updateCenter(); SetShape(entry, s);
			}
			if (ImGui::IsItemActivated()) {
				EditorManager::getInstance().BeginEdit({ parent });
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				EditorManager::getInstance().EndEdit({ parent });
			}
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			if (!entry.isAddVertex) {
				if (ImGui::Button("Reset vertices##CollisionPolyReset")) {
					EditorManager::getInstance().BeginEdit({ parent });

					s.vertices = {};
					SetShape(entry, s);

					TransformComponent* tc = parent->GetComponent<TransformComponent>();
					Renderer::getInstance().polygonEditGizmos->BeginEdit(tc, {}, PolygonEditGizmos::VertexAddMode::Append);
					int id = entry.id;
					entry.polygonEditCallbackID = Renderer::getInstance().polygonEditGizmos->AddChangeCallback(
						[this, id](const std::vector<glm::vec3>& verts) {
							CollisionShapeEntry* e = GetShape(id);
							if (e) this->ApplyLiveShapeUpdate(*e, verts);
						});

					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					entry.isAddVertex = true;
				}

				ImGui::SameLine();
				if (ImGui::Button("Edit vertices##CollisionPolyEditVerts")) {
					EditorManager::getInstance().BeginEdit({ parent });

					std::vector<glm::vec3> localVerts;
					localVerts.reserve(entry.points.size());
					for (auto& p : entry.points) {
						localVerts.push_back(glm::vec3(p[0], p[1], 0.0f));
					}

					TransformComponent* tc = parent->GetComponent<TransformComponent>();
					Renderer::getInstance().polygonEditGizmos->BeginEdit(tc, localVerts, PolygonEditGizmos::VertexAddMode::InsertOnEdge);
					int id = entry.id;
					entry.polygonEditCallbackID = Renderer::getInstance().polygonEditGizmos->AddChangeCallback(
						[this, id](const std::vector<glm::vec3>& verts) {
							CollisionShapeEntry* e = GetShape(id);
							if (e) this->ApplyLiveShapeUpdate(*e, verts);
						});

					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					entry.isAddVertex = true;
				}
			}
			else {
				using AddMode = PolygonEditGizmos::VertexAddMode;
				AddMode mode = Renderer::getInstance().polygonEditGizmos->GetAddMode();
				const char* helpText =
					(mode == AddMode::Append) ? "Click to add, drag to move, right-click to remove" :
					(mode == AddMode::InsertOnEdge) ? "Click highlighted edge to insert, drag to move, right-click to remove" :
					"Drag to move, right-click to remove";
				ImGui::TextWrapped("%s", helpText);
				const auto& editedVerts = Renderer::getInstance().polygonEditGizmos->GetLocalVertices();
				ImGui::Text("Vertices: %d", (int)editedVerts.size());

				ImGui::BeginDisabled(editedVerts.size() < 3);
				if (ImGui::Button("Confirm##Collision")) {
					std::vector<float> newVertices;
					newVertices.reserve(editedVerts.size() * 5);
					for (auto& v : editedVerts) {
						newVertices.insert(newVertices.end(), { v.x, v.y, 0.0f, 0.0f, 0.0f });
					}

					s.vertices = newVertices;
					SetShape(entry, s);

					Renderer::getInstance().polygonEditGizmos->EndEdit();
					Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
					entry.polygonEditCallbackID = -1;
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
					entry.isAddVertex = false;

					EditorManager::getInstance().EndEdit({ parent });
				}
				ImGui::EndDisabled();

				ImGui::SameLine();
				if (ImGui::Button("Cancel##CollisionPolyCancel")) {
					Renderer::getInstance().polygonEditGizmos->EndEdit();
					Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
					entry.polygonEditCallbackID = -1;
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
					entry.isAddVertex = false;

					EditorManager::getInstance().EndEdit({ parent });
				}
			}
		}
		}, entry.currentShape);

	DrawShapePreview(entry);

	if (entry.points.size() < 3) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
		ImGui::TextWrapped(
			"Warning: this shape has fewer than 3 vertices and will not be "
			"registered for broad-phase collision. Add vertices (Edit vertices) "
			"to make it collidable.");
		ImGui::PopStyleColor();
	}

	if (entry.syncWithRenderComponent) {
		ImGui::EndDisabled();
	}
}

void CollisionComponent::ProcessInspectorUI() {
	if (!(parent->HasComponent<RigidBodyComponent>() || parent->HasComponent<SoftBodyComponent>()
		|| parent->HasComponent<FluidComponent>())) {
		bool staticVal = isStatic;
		if (ImGui::Checkbox("Static", &staticVal)) {
			EditorManager::getInstance().BeginEdit({ parent });
			isStatic = staticVal;
			EditorManager::getInstance().EndEdit({ parent });
		}
		ImGui::Separator();
	}

	ImGui::Text("Collision Shapes (%d)", (int)shapes.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Add Shape")) {
		EditorManager::getInstance().BeginEdit({ parent });
		PolygonShape poly;
		poly.vertices = {};
		AddShape(poly);
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Separator();

	int shapeToRemove = -1;
	for (auto& entry : shapes) {
		ImGui::PushID(entry.id);

		bool isResolution = (entry.id == resolutionShapeID);
		bool isInvalid = entry.points.size() < 3;

		std::string label = entry.name;
		if (isInvalid) label += " [!]";
		if (isInvalid) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
		bool open = ImGui::TreeNodeEx((void*)(intptr_t)entry.id,
			ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap,
			"%s", label.c_str());

		if (isInvalid) ImGui::PopStyleColor();

		ImGui::SameLine();
		if (ImGui::SmallButton("Remove")) {
			shapeToRemove = entry.id;
		}

		if (open) {
			ImGui::Indent();
			ProcessShapeEntryUI(entry);
			ImGui::Unindent();
			ImGui::TreePop();
		}

		ImGui::PopID();
		ImGui::Spacing();
	}

	if (shapeToRemove != -1) {
		EditorManager::getInstance().BeginEdit({ parent });
		RemoveShape(shapeToRemove);
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Separator();
	ImGui::Text("Resolution Shape");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Only one shape may drive physical collision resolution.\n"
			"All other shapes still exist for detection but never push objects apart.\n"
			"Select None to keep the object collidable-for-detection without any physical response.");
	}

	CollisionShapeEntry* currentRes = GetResolutionShape();
	if (currentRes && currentRes->points.size() < 3) {
		resolutionShapeID = -1;
		SyncResolutionShapeFields();
	}

	std::string currentLabel = "None";
	for (auto& entry : shapes) {
		if (entry.id == resolutionShapeID) { currentLabel = entry.name; break; }
	}

	if (ImGui::BeginCombo("##ResolutionShapeSelect", currentLabel.c_str())) {
		if (ImGui::Selectable("None", resolutionShapeID == -1)) {
			EditorManager::getInstance().BeginEdit({ parent });
			SetResolutionShapeID(-1);
			EditorManager::getInstance().EndEdit({ parent });
		}
		for (auto& entry : shapes) {
			if (entry.points.size() < 3) continue;

			ImGui::PushID(entry.id);
			bool isSelected = (entry.id == resolutionShapeID);
			if (ImGui::Selectable(entry.name.c_str(), isSelected)) {
				EditorManager::getInstance().BeginEdit({ parent });
				SetResolutionShapeID(entry.id);
				EditorManager::getInstance().EndEdit({ parent });
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	DrawLayerMaskUI("Layer", &collisionLayer);
	ImGui::Spacing();
	DrawLayerMaskUI("Mask", &collisionMask);
}

void CollisionComponent::CopyTo(Object* other) {
	CollisionComponent* target = other->GetComponent<CollisionComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<CollisionComponent>(other));
		target = other->GetComponent<CollisionComponent>();
	}

	target->collisionLayer = collisionLayer;
	target->collisionMask = collisionMask;
	target->isStatic = isStatic;

	while (!target->shapes.empty()) {
		target->RemoveShape(target->shapes.back().id);
	}
	target->resolutionShapeID = -1;

	for (auto& entry : shapes) {
		int newId = target->AddShape(entry.currentShape, entry.name);
		CollisionShapeEntry* newEntry = target->GetShape(newId);
		if (!newEntry) continue;

		newEntry->pendingShape = entry.currentShape;   

		if (entry.syncWithRenderComponent) {
			target->SetSyncWithRenderComponent(*newEntry, true);
		}

		if (entry.id == resolutionShapeID) {
			target->SetResolutionShapeID(newId);
		}
	}

	FluidComponent* fc = other->GetComponent<FluidComponent>();
	if (fc) fc->UpdateCollisionLayerMask();

	target->SetEnabled(Enabled);
}

void CollisionComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);

	w.Write(collisionLayer);
	w.Write(collisionMask);
	w.Write(isStatic);

	w.Write(static_cast<int>(shapes.size()));
	w.Write(resolutionShapeID);

	for (auto& entry : shapes) {
		w.Write(entry.id);
		w.WriteString(entry.name);
		w.Write(entry.syncWithRenderComponent);

		if (!entry.syncWithRenderComponent) {
			if (std::holds_alternative<RectangleShape>(entry.currentShape)) {
				w.Write(static_cast<uint8_t>(1));
				auto& s = std::get<RectangleShape>(entry.currentShape);
				w.Write(s.center);
				w.Write(s.width);
				w.Write(s.height);
			}
			else if (std::holds_alternative<CircleShape>(entry.currentShape)) {
				w.Write(static_cast<uint8_t>(2));
				auto& s = std::get<CircleShape>(entry.currentShape);
				w.Write(s.center);
				w.Write(s.radius);
				w.Write(s.segments);
			}
			else {
				w.Write(static_cast<uint8_t>(0));
				auto& s = std::get<PolygonShape>(entry.currentShape);
				w.WriteArray(s.vertices);
			}
		}
	}
}

void CollisionComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	collisionLayer = r.Read<uint16_t>();
	collisionMask = r.Read<uint16_t>();
	isStatic = r.Read<bool>();

	int shapeCount = r.Read<int>();
	resolutionShapeID = r.Read<int>();

	RenderComponent* rcForCleanup = parent->GetComponent<RenderComponent>();
	for (auto& entry : shapes) {
		if (entry.BAHnode) {
			PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
			entry.BAHnode = nullptr;
		}
		if (entry.renderSyncCallbackID != -1) {
			if (rcForCleanup) rcForCleanup->RemoveOnShapeSetCallback(entry.renderSyncCallbackID);
			entry.renderSyncCallbackID = -1;
		}
		if (entry.polygonEditCallbackID != -1) {
			Renderer::getInstance().polygonEditGizmos->EndEdit();
			Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
			entry.polygonEditCallbackID = -1;
		}
	}

	shapes.clear();
	nextShapeID = 0;

	for (int i = 0; i < shapeCount; i++) {
		CollisionShapeEntry entry;
		entry.id = r.Read<int>();
		entry.name = r.ReadString();
		entry.syncWithRenderComponent = r.Read<bool>();

		nextShapeID = std::max(nextShapeID, entry.id + 1);

		if (!entry.syncWithRenderComponent) {
			uint8_t shapeType = r.Read<uint8_t>();
			if (shapeType == 1) {
				RectangleShape s;
				s.center = r.Read<glm::vec3>();
				s.width = r.Read<float>();
				s.height = r.Read<float>();
				entry.pendingShape = s;
			}
			else if (shapeType == 2) {
				CircleShape s;
				s.center = r.Read<glm::vec3>();
				s.radius = r.Read<float>();
				s.segments = r.Read<int>();
				entry.pendingShape = s;
			}
			else {
				PolygonShape s;
				s.vertices = r.ReadArray<float>();
				entry.pendingShape = s;
			}
		}

		shapes.push_back(std::move(entry));
	}
}

void CollisionComponent::PostLoad() {
	FluidComponent* fc = parent->GetComponent<FluidComponent>();
	TransformComponent* tc = parent->GetComponent<TransformComponent>();

	for (auto& entry : shapes) {
		if (entry.syncWithRenderComponent) {
			SetSyncWithRenderComponent(entry, true);
		}
		else {
			std::visit([&](auto&& s) {
				using T = std::decay_t<decltype(s)>;
				if constexpr (std::is_same_v<T, RectangleShape> || std::is_same_v<T, CircleShape>) {
					s.center = tc ? tc->GetWorldPosition() : s.center;
				}
				}, entry.pendingShape);
			SetShape(entry, entry.pendingShape);
		}
	}

	SyncResolutionShapeFields();

	if (fc) {
		fc->UpdateCollisionLayerMask();

		bool hadAnyNode = false;
		for (auto& entry : shapes) {
			if (entry.BAHnode) {
				PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
				entry.BAHnode = nullptr;
				hadAnyNode = true;
			}
		}
		if (hadAnyNode && tc) {
			tc->RemoveTransformCallback(onTransformCallbackID);
		}
	}
	else {
		calculateBoundingCircle();
	}
}

void CollisionComponent::SetEnabled(bool enabled) {
	if (enabled) {
		for (auto& entry : shapes) {
			if (entry.syncWithRenderComponent == false && entry.points.size() < 3) {
				continue;
			}
			if (!entry.BAHnode) {
				entry.BAHnode = PhysicsEngine::getInstance().RegisterBoundingAreaNode(parent, entry.id, entry.boundingCircle);
			}
		}
		calculateBoundingCircle();

	}
	else {
		for (auto& entry : shapes) {
			if (entry.BAHnode) {
				PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
				entry.BAHnode = nullptr;
			}
		}
	}
}

void CollisionComponent::OnDelete() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc) tc->RemoveTransformCallback(onTransformCallbackID);

	if (physicsChangeEventCallbackID != -1) {
		EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsChangeEventCallbackID);
		physicsChangeEventCallbackID = -1;
	}

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	for (auto& entry : shapes) {
		if (entry.renderSyncCallbackID != -1) {
			if (rc) rc->RemoveOnShapeSetCallback(entry.renderSyncCallbackID);
			entry.renderSyncCallbackID = -1;
		}
		if (entry.polygonEditCallbackID != -1) {
			Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(entry.polygonEditCallbackID);
			entry.polygonEditCallbackID = -1;
		}
		if (entry.BAHnode) {
			PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
			entry.BAHnode = nullptr;
		}
	}

	PhysicsEngine::getInstance().PurgeObjectFromCollisionTracking(parent);
}

void CollisionComponent::calculateBoundingCircle() {
	EngineManager::getInstance().SceneChangeEvent();

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) {
		for (auto& entry : shapes) {
			if (entry.BAHnode) {
				PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
				entry.BAHnode = nullptr;
			}
		}
		return;
	}

	for (auto& entry : shapes) {
		calculateBoundingCircle(entry);
	}
}

void CollisionComponent::calculateBoundingCircle(CollisionShapeEntry& entry) {
	EngineManager::getInstance().SceneChangeEvent();

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;

	if (entry.syncWithRenderComponent == false && entry.points.size() < 3) {
		if (entry.BAHnode) {
			PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent, entry.id);
			entry.BAHnode = nullptr;
		}
		return;
	}

	glm::vec3 center = tc->GetWorldPosition();
	float radius = 0.0f;
	for (auto& p : entry.points) {
		glm::vec3 worldP = tc->ProjectToWorld(glm::vec3(p[0], p[1], 0.0f));
		float dist = glm::distance(center, worldP);
		if (dist > radius) radius = dist;
	}

	entry.boundingCircle.center = center;
	entry.boundingCircle.radius = radius;
	entry.boundingCircle.collisionLayer = collisionLayer;
	entry.boundingCircle.collisionMask = collisionMask;

	if (!isActive) return;  

	if (parent->HasComponent<FluidComponent>()) return;

	if (!entry.BAHnode) {
		entry.BAHnode = PhysicsEngine::getInstance().RegisterBoundingAreaNode(parent, entry.id, entry.boundingCircle);
	}
	else {
		entry.BAHnode->area = entry.boundingCircle;
		entry.BAHnode->recalculateBoundingArea();
		if (entry.BAHnode->parent) entry.BAHnode->parent->recalculateBoundingArea();
	}
}