#include "../../Header Files/Components/CollisionComponent.h"
#include "../../Header Files/Core/Physics/PhysicsEngine.h"
#include "../../Header Files/Core/ObjectManager.h"
#include <glm/gtc/constants.hpp>
#include <array>

CollisionComponent::CollisionComponent(Object* parent) : ComponentBase<CollisionComponent>(parent) {
	Name = "Collision Component";

	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (rc) {
		SetSyncWithRenderComponent(true); 
	}
	else {
		calculateBoundingCircle();
	}

	onTransformCallbackID = parent->GetComponent<TransformComponent>()->AddTransformCallback([this]() {this->calculateBoundingCircle();});

	FluidComponent* fc = parent->GetComponent<FluidComponent>();
	if (!fc) {
		BAHnode = PhysicsEngine::getInstance().RegisterBoundingAreaNode(parent, boundingCircle);
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
			int ps = s.physicsSegments;
			for (int i = 0; i < ps; i++) {
				float theta = 2.0f * glm::pi<float>() * float(i) / float(ps);
				glm::vec3 worldPoint = s.center + glm::vec3(s.radius * std::cos(theta), s.radius * std::sin(theta), 0.0f);
				verts.push_back(tc ? tc->ProjectToWorld(worldPoint, true) : worldPoint);
			}
			return verts;
		}
		}, shape);
}

void CollisionComponent::RebuildFromShape(const std::vector<glm::vec3>& localVerts) {
	points.clear();
	points.reserve(localVerts.size());
	for (int i = 0; i < (int)localVerts.size(); i++) {
		points.push_back({ localVerts[i].x, localVerts[i].y, float(i) });
	}

	edges.clear();
	if (points.size() >= 2) {
		edges.reserve(points.size());
		for (int i = 0; i < (int)points.size(); i++) {
			Edge edge;
			edge.start = glm::vec3(points[i][0], points[i][1], 0);
			edge.end = glm::vec3(points[(i + 1) % points.size()][0], points[(i + 1) % points.size()][1], 0);
			edges.push_back(edge);
		}
	}

	calculateBoundingCircle();
	EngineManager::getInstance().EngineChangeEvent();
}

void CollisionComponent::SetShape(Shape shape) {
	currentShape = shape;
	RebuildFromShape(VerticesFromShape(shape));
}

glm::vec3 CollisionComponent::GetCenter() {
	float A = 0, C_x = 0, C_y = 0;
	int n = (int)points.size();
	if (n < 3) return glm::vec3(0);
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		float shoelace = points[i][0] * points[j][1] - points[j][0] * points[i][1];
		A += shoelace;
		C_x += (points[i][0] + points[j][0]) * shoelace;
		C_y += (points[i][1] + points[j][1]) * shoelace;
	}
	A *= 0.5f;
	if (A == 0) return glm::vec3(0);
	C_x /= (6.0f * A);
	C_y /= (6.0f * A);
	return glm::vec3(C_x, C_y, 0);
}

float CollisionComponent::GetArea() {
	int n = (int)points.size();
	if (n < 3) return 0.0f;
	float area = 0.0f;
	for (int i = 0; i < n; i++) {
		int j = (i + 1) % n;
		area += points[i][0] * points[j][1];
		area -= points[j][0] * points[i][1];
	}
	return std::abs(area) * 0.5f;
}

void CollisionComponent::Draw() {
	if (!Enabled) return;
	if (points.size() < 3) return;

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;

	std::vector<glm::vec3> worldPoints;
	worldPoints.reserve(points.size());
	for (auto& p : points) {
		worldPoints.push_back(tc->ProjectToWorld(glm::vec3(p[0], p[1], 0.0f)));
	}

	const glm::vec4 fillColor(0.3f, 0.7f, 1.0f, 0.25f);
	const glm::vec4 outlineColor(0.3f, 0.7f, 1.0f, 0.85f);

	Renderer::getInstance().DrawFilledPolygon(worldPoints, fillColor, outlineColor, 1.5f);
}

void CollisionComponent::SetSyncWithRenderComponent(bool sync) {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (!rc) {
		syncWithRenderComponent = false;
		return;
	}

	if (sync) {
		syncWithRenderComponent = true;
		if (renderSyncCallbackID == -1) {
			renderSyncCallbackID = rc->AddOnShapeSetCallback([this]() {
				this->SyncFromRenderComponent();
				});
		}
		SyncFromRenderComponent();
	}
	else {
		if (renderSyncCallbackID != -1) {
			rc->RemoveOnShapeSetCallback(renderSyncCallbackID);
			renderSyncCallbackID = -1;
		}
		syncWithRenderComponent = false;
	}
}

void CollisionComponent::SyncFromRenderComponent() {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();
	if (!rc) return;

	currentShape = rc->currentShape;
	points = rc->points;
	edges = rc->edges;

	calculateBoundingCircle();
	EngineManager::getInstance().EngineChangeEvent();
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
			*layer ^= (1 << i);
			boundingCircle.collisionLayer = collisionLayer;
			boundingCircle.collisionMask = collisionMask;
			BAHnode->area.collisionLayer = collisionLayer;
			BAHnode->area.collisionMask = collisionMask;
			FluidComponent* fc = parent->GetComponent<FluidComponent>();
			if (fc) fc->UpdateCollisionLayerMask();
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

void CollisionComponent::ProcessInspectorUI() {
	RenderComponent* rc = parent->GetComponent<RenderComponent>();

	if (rc) {
		bool sync = syncWithRenderComponent;
		if (ImGui::Checkbox("Sync with Render Component", &sync)) {
			SetSyncWithRenderComponent(sync);
		}
		ImGui::Separator();

		if (syncWithRenderComponent) {
			SyncFromRenderComponent();
		}
	}

	if (syncWithRenderComponent) {
		ImGui::BeginDisabled();
	}

	ImGui::Text("Shape");
	ImGui::SameLine();

	const char* shapeLabel = std::visit([](auto&& s) -> const char* {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape>) return "Rectangle";
		else if constexpr (std::is_same_v<T, CircleShape>)  return "Circle";
		else                                                  return "Polygon";
		}, currentShape);

	if (ImGui::BeginCombo("##CollisionShapeSelect", shapeLabel)) {
		if (ImGui::Selectable("Rectangle", std::holds_alternative<RectangleShape>(currentShape))) {
			RectangleShape rect;
			rect.width = rect.height = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			rect.center = tc ? tc->GetWorldPosition() : GetCenter();
			if (Renderer::getInstance().polygonEditGizmos->IsEditing())
				Renderer::getInstance().polygonEditGizmos->EndEdit();
			isAddVertex = false;
			SetShape(rect);
		}
		if (ImGui::Selectable("Circle", std::holds_alternative<CircleShape>(currentShape))) {
			CircleShape cir;
			cir.radius = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			cir.center = tc ? tc->GetWorldPosition() : GetCenter();
			if (Renderer::getInstance().polygonEditGizmos->IsEditing())
				Renderer::getInstance().polygonEditGizmos->EndEdit();
			isAddVertex = false;
			SetShape(cir);
		}
		if (ImGui::Selectable("Polygon", std::holds_alternative<PolygonShape>(currentShape))) {
			PolygonShape poly;
			poly.vertices = {};
			SetShape(poly);

			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			Renderer::getInstance().polygonEditGizmos->BeginEdit(tc);

			EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
			isAddVertex = true;
		}
		ImGui::EndCombo();
	}

	std::visit([this](auto&& s) {
		using T = std::decay_t<decltype(s)>;

		if constexpr (std::is_same_v<T, RectangleShape>) {
			float dims[2] = { s.width, s.height };
			ImGui::Text("  Size");
			ImGui::SameLine();
			if (ImGui::InputFloat2("##CollisionRectSize", dims, "%.3f m")) {
				s.width = std::max(0.01f, dims[0]);
				s.height = std::max(0.01f, dims[1]);
				TransformComponent* tc = parent->GetComponent<TransformComponent>();
				s.center = tc ? tc->GetWorldPosition() : GetCenter();
				SetShape(s);
			}
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			auto updateCenter = [&]() {
				TransformComponent* tc = parent->GetComponent<TransformComponent>();
				s.center = tc ? tc->GetWorldPosition() : GetCenter();
				};

			float r = s.radius;
			ImGui::Text("  Radius");
			ImGui::SameLine();
			if (ImGui::InputFloat("##CollisionCircleRadius", &r, 0.0f, 0.0f, "%.3f m")) {
				s.radius = std::max(0.01f, r); updateCenter(); SetShape(s);
			}

			int pseg = s.physicsSegments;
			ImGui::Text("  Sim Seg");
			ImGui::SameLine();
			if (ImGui::InputInt("##CollisionCirclePhysSeg", &pseg)) {
				s.physicsSegments = std::max(3, pseg); updateCenter(); SetShape(s);
			}
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			if (!isAddVertex) {
				if (ImGui::Button("Reset vertices##CollisionPolyReset")) {
					s.vertices = {};
					SetShape(s);

					TransformComponent* tc = parent->GetComponent<TransformComponent>();
					Renderer::getInstance().polygonEditGizmos->BeginEdit(tc, {}, true);

					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					isAddVertex = true;
				}

				ImGui::SameLine();
				if (ImGui::Button("Edit vertices##CollisionPolyEditVerts")) {
					std::vector<glm::vec3> localVerts;
					localVerts.reserve(points.size());
					for (auto& p : points) {
						localVerts.push_back(glm::vec3(p[0], p[1], 0.0f));
					}
					
					TransformComponent * tc = parent->GetComponent<TransformComponent>();
					Renderer::getInstance().polygonEditGizmos->BeginEdit(tc, localVerts, false);
					
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					isAddVertex = true;
					
				}
			}
			else {
				bool canAdd = Renderer::getInstance().polygonEditGizmos->CanAddVertices();
				ImGui::Text(canAdd
					 ? "Click to add, drag to move, right-click to remove"
					 : "Drag to move, right-click to remove");
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
					SetShape(s);
					
					Renderer::getInstance().polygonEditGizmos->EndEdit();
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
					isAddVertex = false;
					
				}
				ImGui::EndDisabled();
				
				ImGui::SameLine();
				if (ImGui::Button("Cancel##CollisionPolyCancel")) {
					Renderer::getInstance().polygonEditGizmos->EndEdit();
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
					isAddVertex = false;
				}
			}
		}
		}, currentShape);

	ImGui::Spacing();
	ImGui::Text("Collision Preview");

	const ImVec2 previewSize(100.0f, 100.0f);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImU32 outlineCol = IM_COL32(80, 200, 120, 220);

	draw->AddRectFilled(pos, ImVec2(pos.x + previewSize.x, pos.y + previewSize.y), IM_COL32(40, 40, 40, 255), 4.0f);

	{
		const float cx = pos.x + previewSize.x * 0.5f;
		const float cy = pos.y + previewSize.y * 0.5f;

		std::vector<std::pair<float, float>> displayVerts;
		for (auto& p : points) displayVerts.push_back({ p[0], p[1] });

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
	}

	ImGui::Dummy(previewSize);

	if (syncWithRenderComponent) {
		ImGui::EndDisabled();
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

	if (syncWithRenderComponent) {
		target->SetSyncWithRenderComponent(true);
	}
	else {
		target->SetSyncWithRenderComponent(false);
		target->SetShape(currentShape);
	}

	FluidComponent* fc = other->GetComponent<FluidComponent>();
	if (fc) fc->UpdateCollisionLayerMask();
}

std::unique_ptr<Component> CollisionComponent::Clone(Object* parent) {
	std::unique_ptr<CollisionComponent> comp = std::make_unique<CollisionComponent>(parent);
	comp->collisionLayer = collisionLayer;
	comp->collisionMask = collisionMask;

	if (syncWithRenderComponent) {
		comp->SetSyncWithRenderComponent(true);
	}
	else {
		comp->SetSyncWithRenderComponent(false);
		comp->pendingShape = currentShape;
	}

	comp->SetEnabled(false);
	return comp;
}

void CollisionComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);

	w.Write(collisionLayer);
	w.Write(collisionMask);
	w.Write(syncWithRenderComponent);

	if (!syncWithRenderComponent) {
		if (std::holds_alternative<RectangleShape>(currentShape)) {
			w.Write(static_cast<uint8_t>(1));
			auto& s = std::get<RectangleShape>(currentShape);
			w.Write(s.center);
			w.Write(s.width);
			w.Write(s.height);
		}
		else if (std::holds_alternative<CircleShape>(currentShape)) {
			w.Write(static_cast<uint8_t>(2));
			auto& s = std::get<CircleShape>(currentShape);
			w.Write(s.center);
			w.Write(s.radius);
			w.Write(s.segments);
			w.Write(s.physicsSegments);
		}
		else {
			w.Write(static_cast<uint8_t>(0));
			auto& s = std::get<PolygonShape>(currentShape);
			w.WriteArray(s.vertices);
		}
	}
}

void CollisionComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	collisionLayer = r.Read<uint16_t>();
	collisionMask = r.Read<uint16_t>();

	syncWithRenderComponent = r.Read<bool>();

	if (!syncWithRenderComponent) {
		uint8_t shapeType = r.Read<uint8_t>();
		if (shapeType == 1) {
			RectangleShape s;
			s.center = r.Read<glm::vec3>();
			s.width = r.Read<float>();
			s.height = r.Read<float>();
			pendingShape = s;
		}
		else if (shapeType == 2) {
			CircleShape s;
			s.center = r.Read<glm::vec3>();
			s.radius = r.Read<float>();
			s.segments = r.Read<int>();
			s.physicsSegments = r.Read<int>();
			pendingShape = s;
		}
		else {
			PolygonShape s;
			s.vertices = r.ReadArray<float>();
			pendingShape = s;
		}
	}
}

void CollisionComponent::PostLoad() {
	FluidComponent* fc = parent->GetComponent<FluidComponent>();

	if (syncWithRenderComponent) {
		SetSyncWithRenderComponent(true);
	}
	else {
		TransformComponent* tc = parent->GetComponent<TransformComponent>();
		std::visit([&](auto&& s) {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, RectangleShape> || std::is_same_v<T, CircleShape>) {
				s.center = tc ? tc->GetWorldPosition() : s.center;
			}
			}, pendingShape);
		SetShape(pendingShape);
	}

	if (fc) {
		fc->UpdateCollisionLayerMask();
		if (BAHnode) {
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			if (tc) tc->RemoveTransformCallback(onTransformCallbackID);
			PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
		}
	}
	else {
		calculateBoundingCircle();
	}
}

void CollisionComponent::SetEnabled(bool enabled) {
	if (enabled) {
		if (!BAHnode) {
			BAHnode = PhysicsEngine::getInstance().RegisterBoundingAreaNode(parent, boundingCircle);
		}
		calculateBoundingCircle();
	}
	else {
		PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
		BAHnode = nullptr;
	}
}

void CollisionComponent::OnDelete() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (tc) tc->RemoveTransformCallback(onTransformCallbackID);

	if (renderSyncCallbackID != -1) {
		RenderComponent* rc = parent->GetComponent<RenderComponent>();
		if (rc) rc->RemoveOnShapeSetCallback(renderSyncCallbackID);
		renderSyncCallbackID = -1;
	}

	PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
}

void CollisionComponent::calculateBoundingCircle() {
	EngineManager::getInstance().EngineChangeEvent();

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) {
		PhysicsEngine::getInstance().UnRegisterBoundingAreaNode(parent);
		BAHnode = nullptr;
		return;
	}

	glm::vec3 center = tc->GetWorldPosition();

	float radius = 0;
	for (int i = 0; i < points.size(); i++) {
		glm::vec3 p = glm::vec3(points[i][0], points[i][1], 0);
		glm::vec3 worldP = tc->ProjectToWorld(p);
		float dist = glm::distance(center, worldP);
		if (dist > radius) radius = dist;
	}

	boundingCircle.center = center;
	boundingCircle.radius = radius;
	boundingCircle.collisionLayer = collisionLayer;
	boundingCircle.collisionMask = collisionMask;

	BAHnode = PhysicsEngine::getInstance().root.searchFor(parent);
	if (BAHnode != nullptr) {
		BAHnode->area = boundingCircle;
		BAHnode->recalculateBoundingArea();
		if (BAHnode->parent != nullptr)
			BAHnode->parent->recalculateBoundingArea();
	}
}