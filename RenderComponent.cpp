#include "RenderComponent.h"
#include "VertexComponent.h"
#include "ObjectManager.h"

RenderComponent::RenderComponent(Object* parent, std::vector<float> vertices, Shader shader, std::string texture_path) : ComponentBase<RenderComponent>(parent) {
	Name = "Render Component";

	CanRemove = false;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	Vertices = vertices;
	Indices = Triangulate(vertices);
	this->shader = shader;

	points.clear();
	edges.clear();
	for (int i = 0; i < vertices.size(); i += 2)
	{
		points.push_back(std::vector<float> {
			vertices[i],
			vertices[i + 1],
			float(int(i / 5))
		});
		i += 3;
	}

	for (int i = 0; i < points.size(); i++)
	{
		glm::vec3 p1 = glm::vec3(points[i][0], points[i][1], 0);
		glm::vec3 p2 = glm::vec3(points[(i + 1) % points.size()][0], points[(i + 1) % points.size()][1], 0);
		Edge edge = Edge();
		edge.start = p1;
		edge.end = p2;
		edges.push_back(edge);
	}

	glGenVertexArrays(1, &this->VAO);
	glBindVertexArray(this->VAO);

	glGenBuffers(1, &this->VBO);
	glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
	glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(float), Vertices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &this->EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(unsigned int), Indices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// Setup texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load textures
	SetTexture(texture_path);
}

int RenderComponent::AddOnShapeSetCallback(std::function<void()> func) {
	shapeCallbackID += 1;
	OnShapeSetCallbacks[shapeCallbackID] = func;
	return shapeCallbackID;
}

void RenderComponent::RemoveOnShapeSetCallback(int ID) {
	OnShapeSetCallbacks.erase(ID);
}

void RenderComponent::SetShape(Shape shape) {
	currentShape = shape;
	auto verts = VerticesFromShape(shape);

	if (std::holds_alternative<CircleShape>(shape)) {
		auto& circle = std::get<CircleShape>(shape);

		UpdateShape(verts, TriangulateCircle(circle.segments)); 

		edges.clear();
		TransformComponent* tc = parent->HasComponent<TransformComponent>()
			? parent->GetComponent<TransformComponent>() : nullptr;

		int ps = circle.physicsSegments;
		for (int i = 0; i < ps; i++) {
			float theta1 = 2.0f * glm::pi<float>() * float(i) / float(ps);
			float theta2 = 2.0f * glm::pi<float>() * float(i + 1) / float(ps);
			glm::vec3 wp1 = circle.center + glm::vec3(circle.radius * std::cos(theta1), circle.radius * std::sin(theta1), 0.0f);
			glm::vec3 wp2 = circle.center + glm::vec3(circle.radius * std::cos(theta2), circle.radius * std::sin(theta2), 0.0f);
			Edge e;
			e.start = tc ? tc->ProjectToWorld(wp1, true) : wp1;
			e.end = tc ? tc->ProjectToWorld(wp2, true) : wp2;
			edges.push_back(e);
		}
	}
	else {
		UpdateShape(verts, Triangulate(verts));
	}

	if (!std::holds_alternative<PolygonShape>(shape)) {
		for (auto& [id, func] : OnShapeSetCallbacks) {
			func();
		}
	}
}

void RenderComponent::SetTexture(std::string texture_path) {
	this->texture_path = texture_path;
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true);

	if (this->TextureID != 0) {
		glDeleteTextures(1, &this->TextureID);
	}

	unsigned char* data = stbi_load(texture_path.c_str(), &width, &height, &nrChannels, 0);
	glGenTextures(1, &this->TextureID);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->TextureID);

	if (data) {
		if (nrChannels == 3) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		else if (nrChannels == 4) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
}


void RenderComponent::Draw() {
	
	this->shader.use();
	if (!Enabled)
		return;

	this->shader.setVec4D("aColor", this->color);

	if (this->TextureID != 0) {
		glActiveTexture(this->TextureID);
		glBindTexture(GL_TEXTURE_2D, this->TextureID);
	}

	glBindVertexArray(this->VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
	glDrawElements(GL_TRIANGLES, Indices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

float calcTriangleArea(std::vector<float> a, std::vector<float> b, std::vector<float> c) {
	return 0.5f * std::abs((a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1])));
}

std::vector<unsigned int> RenderComponent::TriangulateCircle(int segments) {
	std::vector<unsigned int> indices;
	for (int i = 0; i < segments; i++) {
		indices.push_back(i);
		indices.push_back((i + 1) % segments);
		indices.push_back(segments); 
	}
	return indices;
}

std::vector<unsigned int> RenderComponent::Triangulate(std::vector<float> vertices) {
	if (vertices.size() == 0) {
		return {};
	}

	points.clear();
	for (int i = 0; i < vertices.size(); i += 2)
	{
		points.push_back(std::vector<float> {
			vertices[i],
				vertices[i + 1],
				float(int(i / 5))
		});
		i += 3;
	}

	std::vector<unsigned int> indices;

	unsigned int earIndex = 0;

	std::vector<float> ear = points[0];
	std::vector<float> prev = points[points.size() - 1];
	std::vector<float> next = points[1];


	while (points.size() > 3)
	{
		bool ValidEar = true;
		glm::vec2 v1 = glm::vec2(ear[0] - prev[0], ear[1] - prev[1]);
		glm::vec2 v2 = glm::vec2(ear[0] - next[0], ear[1] - next[1]);

		float angle = glm::angle(glm::normalize(v1), glm::normalize(v2));

		for (int i = 0; i < points.size(); i++)
		{
			if (points[i][2] != ear[2] && points[i][2] != prev[2] && points[i][2] != next[2]) {
				float areafull = calcTriangleArea(prev, ear, next);
				float area1 = calcTriangleArea(points[i], prev, ear);
				float area2 = calcTriangleArea(points[i], ear, next);
				float area3 = calcTriangleArea(points[i], prev, next);

				if (abs(area1 + area2 + area3 - areafull) < 0.0001) {
					ValidEar = false;
					break;
				}
			}
		}
		if (angle > glm::pi<float>()) {
			ValidEar = false;
			std::cout << "Angle is concave, not an ear." << std::endl;
		}

		if (ValidEar) {
			indices.push_back((unsigned int)prev[2]);
			indices.push_back((unsigned int)ear[2]);
			indices.push_back((unsigned int)next[2]);

			points.erase(points.begin() + earIndex);
			earIndex = 0;
			ear = points[0];
			prev = points[points.size() - 1];
			next = points[1];

		}
		else {
			earIndex++;
			if (earIndex >= points.size()) {
				std::cout << "No valid ear found, polygon might be malformed." << std::endl;
				break;
			}
			ear = points[earIndex];
			prev = points[(earIndex - 1 < 0) ? points.size() - 1 : earIndex - 1];
			next = points[(earIndex + 1 >= points.size()) ? 0 : earIndex + 1];
		}
	}

	indices.push_back((unsigned int)points[2][2]);
	indices.push_back((unsigned int)points[0][2]);
	indices.push_back((unsigned int)points[1][2]);

	return indices;
}

std::vector<float> RenderComponent::VerticesFromShape(Shape& shape) {
	return std::visit([this](auto&& s) -> std::vector<float> {
		using T = std::decay_t<decltype(s)>;

		if constexpr (std::is_same_v<T, PolygonShape>) {
			return s.vertices;
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

			std::array<glm::vec2, 4> uvs = {
				glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)
			};

			TransformComponent* tc = parent->HasComponent<TransformComponent>()
				? parent->GetComponent<TransformComponent>() : nullptr;

			std::vector<float> verts;
			for (int i = 0; i < 4; i++) {
				glm::vec3 p = tc ? tc->ProjectToWorld(worldCorners[i], true) : worldCorners[i];
				verts.insert(verts.end(), { p.x, p.y, 0.0f, uvs[i].x, uvs[i].y });
			}
			return verts;
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			TransformComponent* tc = parent->HasComponent<TransformComponent>()
				? parent->GetComponent<TransformComponent>() : nullptr;

			std::vector<float> verts;
			for (int i = 0; i < s.segments; ++i) {
				float theta = 2.0f * glm::pi<float>() * float(i) / float(s.segments);

				glm::vec3 worldPoint = s.center + glm::vec3(
					s.radius * std::cos(theta),
					s.radius * std::sin(theta),
					0.0f
				);

				glm::vec3 p = tc ? tc->ProjectToWorld(worldPoint, true) : worldPoint;

				float u = (std::cos(theta) + 1.0f) * 0.5f;
				float v = (std::sin(theta) + 1.0f) * 0.5f;
				verts.insert(verts.end(), { p.x, p.y, 0.0f, u, v });
			}
			glm::vec3 centerLocal = tc ? tc->ProjectToWorld(s.center, true) : s.center;
			verts.insert(verts.end(), { centerLocal.x, centerLocal.y, 0.0f, 0.5f, 0.5f });

			return verts;
		}
		}, shape);
}

float RenderComponent::GetArea() {
	float totalArea = 0;
	for (int i = 0; i < Indices.size(); i+=3)
	{
	 	totalArea += calcTriangleArea(points[Indices[i]], points[Indices[i + 1]], points[Indices[i + 2]]);
	}

	return totalArea;
}

glm::vec3 RenderComponent::GetCenter() {
	float A = 0;
	float C_x = 0;
	float C_y = 0;

	int n = points.size();
	for (int i = 0; i < n; i++)
	{
		int j = (i + 1) % n;
		float shoelace = points[i][0] * points[j][1] - points[j][0] * points[i][1];
		A += shoelace;
		C_x += (points[i][0] + points[j][0]) * shoelace;
		C_y += (points[i][1] + points[j][1]) * shoelace;
	}

	A = A / 2.0;
	if (A == 0) {
		return glm::vec3(0, 0, 0);
	}

	C_x = C_x / (6.0 * A);
	C_y = C_y / (6.0 * A);

	return glm::vec3(C_x, C_y, 0);
}

void RenderComponent::UpdateShape(std::vector<float> vertices, std::vector<unsigned int> indices) {
	Vertices = vertices;
	Indices = indices;

	points.clear();
	for (int i = 0; i < vertices.size(); i += 5) {
		points.push_back({ vertices[i], vertices[i + 1], float(i / 5) });
	}

	if (!std::holds_alternative<CircleShape>(currentShape)) {
		edges.clear();
		for (int i = 0; i < points.size(); i++) {
			Edge edge;
			edge.start = glm::vec3(points[i][0], points[i][1], 0);
			edge.end = glm::vec3(points[(i + 1) % points.size()][0], points[(i + 1) % points.size()][1], 0);
			edges.push_back(edge);
		}
	}

	glBindVertexArray(this->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
	glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(float), Vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(unsigned int), Indices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

bool RenderComponent::IsInsideShape(glm::vec3 point) {
	int cnt = 0;
	for (int i = 0; i < edges.size(); i++)
	{
		std::vector<float> point1 = std::vector<float>{ edges[i].start.x, edges[i].start.y };
		std::vector<float> point2 = std::vector<float>{ edges[i].end.x, edges[i].end.y };
		bool ycheck = (point.y < point1[1]) != (point.y < point2[1]);
		bool xcheck = point.x < point1[0] + ((point.y - point1[1]) / (point2[1] - point1[1])) * (point2[0] - point1[0]);

		if (xcheck && ycheck) {
			cnt += 1;
		}
	}

	return cnt % 2 == 1;
}

void RenderComponent::CopyTo(Object* other) {
	RenderComponent* target = other->GetComponent<RenderComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<RenderComponent>(other, std::vector<float> {}, other->shader, texture_path));
		target = other->GetComponent<RenderComponent>();
	}

	target->z_index = z_index;
	target->SetTexture(texture_path);
	target->color = color;
}

void RenderComponent::ProcessInspectorUI() {
	ImGui::Text("Texture");
	ImGui::SameLine();
	char selected_texture_path[128] = "None (click to choose...)";
	if (!texture_path.empty()) {
#if defined(_MSC_VER)
		strcpy_s(selected_texture_path, texture_path.c_str());
#else
		strncpy(selected_texture_path, texture_path.c_str(), sizeof(selected_texture_path) - 1);
#endif
	}
	ImGui::InputText("##Texture path", selected_texture_path, IM_ARRAYSIZE(selected_texture_path), ImGuiInputTextFlags_ReadOnly);
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (ImGui::IsItemClicked()) {
		IGFD::FileDialogConfig config;
		config.path = ".";
		config.countSelectionMax = 1;
		ImGuiFileDialog::Instance()->OpenDialog("ChooseTexture", "Choose Texture", ".png,.jpeg", config);
	}
	if (!initialized) {
		auto* places = ImGuiFileDialog::Instance()->GetPlacesGroupPtr("Devices");
		if (places) { places->AddPlace("D: ", "D:\\", true); initialized = true; }
	}
	if (ImGuiFileDialog::Instance()->Display("ChooseTexture", 32, ImVec2(100, 200))) {
		if (ImGuiFileDialog::Instance()->IsOk())
			SetTexture(ImGuiFileDialog::Instance()->GetFilePathName());
		ImGuiFileDialog::Instance()->Close();
	}

	ImGui::Separator();

	ImGui::Text("Color");
	ImGui::SameLine();
	float displayColor[4] = { color.x, color.y, color.z, color.a };
	if (ImGui::ColorEdit4("##Color", displayColor))
		this->color = glm::vec4(displayColor[0], displayColor[1], displayColor[2], displayColor[3]);

	ImGui::Separator();

	ImGui::Text("Shape");
	ImGui::SameLine();

	const char* shapeLabel = std::visit([](auto&& s) -> const char* {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape>) return "Rectangle";
		else if constexpr (std::is_same_v<T, CircleShape>)  return "Circle";
		else                                                  return "Polygon";
		}, currentShape);

	if (ImGui::BeginCombo("##ShapeSelect", shapeLabel)) {
		if (ImGui::Selectable("Rectangle", std::holds_alternative<RectangleShape>(currentShape))) {
			RectangleShape rect;
			rect.width = rect.height = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			rect.center = tc ? tc->GetWorldPosition() : GetCenter();

			VertexComponent* vc = parent->GetComponent<VertexComponent>();
			if (vc) {
				vc->SetEnabled(false);
				vc->RemoveAllVertex();
			}

			SetShape(rect);
		}
		if (ImGui::Selectable("Circle", std::holds_alternative<CircleShape>(currentShape))) {
			CircleShape cir;
			cir.radius = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			cir.center = tc ? tc->GetWorldPosition() : GetCenter();

			VertexComponent* vc = parent->GetComponent<VertexComponent>();
			if (vc) {
				vc->SetEnabled(false);
				vc->RemoveAllVertex();
			}

			SetShape(cir);
		}
		if (ImGui::Selectable("Polygon", std::holds_alternative<PolygonShape>(currentShape))) {
			PolygonShape poly;
			poly.vertices = {};

			VertexComponent* vc = parent->GetComponent<VertexComponent>();
			if (vc) {
				vc->SetEnabled(true);
				vc->RemoveAllVertex();
			}
			else {
				parent->AddComponent(std::make_unique<VertexComponent>(parent));
				vc = parent->GetComponent<VertexComponent>();
				vc->RemoveAllVertex();
			}

			SetShape(poly);

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
			if (ImGui::InputFloat2("##RectSize", dims, "%.3f m")) {
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
			if (ImGui::InputFloat("##CircleRadius", &r, 0.0f, 0.0f, "%.3f m"))
			{
				s.radius = std::max(0.01f, r); updateCenter(); SetShape(s);
			}

			int seg = s.segments;
			ImGui::Text("  Segments");
			ImGui::SameLine();
			if (ImGui::InputInt("##CircleSeg", &seg))
			{
				s.segments = std::max(3, seg); updateCenter(); SetShape(s);
			}

			int pseg = s.physicsSegments;
			ImGui::Text("  Sim Seg");
			ImGui::SameLine();
			if (ImGui::InputInt("##CirclePhysSeg", &pseg))
			{
				s.physicsSegments = std::max(3, pseg); updateCenter(); SetShape(s);
			}
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			if (!isAddVertex) {
				if (ImGui::Button("Reset vertices##PolyReset"))
				{
					s.vertices = {};
					VertexComponent* vc = parent->GetComponent<VertexComponent>();
					if (vc) {
						vc->RemoveAllVertex();
					}
					SetShape(s);
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					isAddVertex = true;
				}
			}
			else {
				ImGui::Text("Click on the screen to add vertices");
				if (ImGui::Button("Confirm")) {
					if (ObjectManager::getInstance().vertexPoints.size() < 3) return;

					parent->GetComponent<VertexComponent>()->SetVertexPoints(ObjectManager::getInstance().vertexPoints);
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);

					s.vertices = ObjectManager::getInstance().vertices;
					SetShape(s);
					TransformComponent* tc = parent->GetComponent<TransformComponent>();
					tc->SetRotationCenter(GetCenter());
					tc->SetOriginTransform(Camera::getInstance().viewMatrixInverse);

					SoftBodyComponent* sb = parent->GetComponent<SoftBodyComponent>();
					if (sb) sb->RebuildMassAggregate();

					ObjectManager::getInstance().vertexPoints.clear();
					ObjectManager::getInstance().vertices.clear();
					isAddVertex = false;
				}
			}
		}
		}, currentShape);

	// Mini preview
	ImGui::Spacing();
	ImGui::Text("Preview");

	const ImVec2 previewSize(100.0f, 100.0f);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.a));
	ImU32 outlineCol = IM_COL32(160, 160, 160, 200);

	draw->AddRectFilled(pos, ImVec2(pos.x + previewSize.x, pos.y + previewSize.y), IM_COL32(40, 40, 40, 255), 4.0f);

	std::visit([&](auto&& s) {
		using T = std::decay_t<decltype(s)>;
		const float cx = pos.x + previewSize.x * 0.5f;
		const float cy = pos.y + previewSize.y * 0.5f;

		if constexpr (std::is_same_v<T, RectangleShape>) {
			float aspect = (s.height > 0.f) ? s.width / s.height : 1.f;
			float hw, hh;
			const float maxHalf = previewSize.x * 0.4f;
			if (aspect >= 1.f) { hw = maxHalf; hh = maxHalf / aspect; }
			else { hh = maxHalf; hw = maxHalf * aspect; }
			draw->AddRectFilled({ cx - hw, cy - hh }, { cx + hw, cy + hh }, fillCol);
			draw->AddRect({ cx - hw, cy - hh }, { cx + hw, cy + hh }, outlineCol, 0.0f, 0, 1.5f);
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			const float r = previewSize.x * 0.38f;
			draw->AddCircleFilled({ cx, cy }, r, fillCol, s.segments);
			draw->AddCircle({ cx, cy }, r, outlineCol, s.segments, 1.5f);
			draw->AddCircle({ cx, cy }, r, IM_COL32(80, 140, 255, 140), s.physicsSegments, 1.0f);
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			std::vector<std::pair<float, float>> displayVerts;

			for (int i = 0; i + 4 < (int)s.vertices.size(); i += 5)
				displayVerts.push_back({ s.vertices[i], s.vertices[i + 1] });

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

				draw->AddConvexPolyFilled(pts.Data, pts.Size, fillCol);
				draw->AddPolyline(pts.Data, pts.Size, outlineCol, ImDrawFlags_Closed, 1.5f);
			}

			int vertCount = (int)s.vertices.size() / 5;
			char badge[32];
			snprintf(badge, sizeof(badge), "%d verts", vertCount);
			draw->AddText({ pos.x + 4, pos.y + 4 }, IM_COL32(200, 200, 200, 180), badge);
		}
		}, currentShape);

	ImGui::Dummy(previewSize);

	ImGui::Separator();

	ImGui::Text("Z index");
	ImGui::SameLine();
	ImGui::InputInt("##ZIndex", &z_index);
}

void RenderComponent::OnDelete() {
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteVertexArrays(1, &VAO);
}
