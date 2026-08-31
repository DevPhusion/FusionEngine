#include "../../Header Files/Components/RenderComponent.h"
#include "../../Header Files/Core/ObjectManager.h"
#include "../../Header Files/Core/Files/FileDialog.h"
#include "../../Header Files/Core/Editor/EditorField.h"

RenderComponent::RenderComponent(Object* parent, std::vector<float> vertices, Shader shader, std::string texture_path) : ComponentBase<RenderComponent>(parent) {
	Name = "Render Component";

	bool headless = EngineManager::getInstance().isHeadless;

	if (!headless) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

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

	if (!headless) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
	}
	glResourcesReady = !headless;

	// Load textures
	SetTexture(texture_path);

	physicsChangeEventCallbackID = EngineManager::getInstance().AddPhysicsModeChangedEvent([this]() {
		if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate && polygonEditCallbackID != -1) {
			Renderer::getInstance().polygonEditGizmos->EndEdit();
			Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(polygonEditCallbackID);
			polygonEditCallbackID = -1;
			EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
			isAddVertex = false;
		}
		});
}

void RenderComponent::EnsureGLResources() {
	if (glResourcesReady) return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(float), Vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(unsigned int), Indices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glResourcesReady = true;
	std::string pendingTexPath = texture_path;
	texture_path = "";
	SetTexture(pendingTexPath);
}

void RenderComponent::ForceRecreateGLResources() {
	if (glResourcesReady) {
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
		VAO = VBO = EBO = 0;

		if (TextureID != 0) {
			if (texture_path != "") {
				auto& cache = TextureCache();
				auto it = cache.find(texture_path);
				if (it != cache.end() && --it->second.second <= 0) {
					glDeleteTextures(1, &it->second.first);
					cache.erase(it);
				}
			}
			else {
				glDeleteTextures(1, &TextureID);
			}
		}
		TextureID = 0;
	}

	glResourcesReady = false;
	EnsureGLResources();   
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

		int ps = circle.segments;
		for (int i = 0; i < ps; i++) {
			float theta1 = 2.0f * glm::pi<float>() * float(i) / float(ps);
			float theta2 = 2.0f * glm::pi<float>() * float(i + 1) / float(ps);
			glm::vec3 wp1 = circle.center + glm::vec3(circle.radius * std::cos(theta1), circle.radius * std::sin(theta1), 0.0f);
			glm::vec3 wp2 = circle.center + glm::vec3(circle.radius * std::cos(theta2), circle.radius * std::sin(theta2), 0.0f);
			Edge e;
			e.start = tc ? tc->ProjectToWorld(wp1, true, false) : wp1;
			e.end = tc ? tc->ProjectToWorld(wp2, true, false) : wp2;
			edges.push_back(e);
		}
	}
	else {
		UpdateShape(verts, Triangulate(verts));
	}

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	tc->SetRotationCenter(GetCenter());
	tc->worldMatrixDirty = true;
	
	EngineManager::getInstance().SceneChangeEvent();
	for (auto& [id, func] : OnShapeSetCallbacks) {
		func();
	}
}

void RenderComponent::ApplyLiveShapeUpdate(const std::vector<glm::vec3>& verts) {
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
	SetShape(s);
}

void RenderComponent::SetTexture(std::string texture_path) {
	if (EngineManager::getInstance().isHeadless && !glResourcesReady) {   
		this->texture_path = texture_path;
		this->TextureID = 0;
		return;
	}

	if (this->texture_path != "" && this->TextureID != 0) {
		auto& cache = TextureCache();
		auto oldIt = cache.find(this->texture_path);
		if (oldIt != cache.end()) {
			if (--oldIt->second.second <= 0) {
				glDeleteTextures(1, &oldIt->second.first);
				cache.erase(oldIt);
			}
		}
	}

	this->texture_path = texture_path;
	if (texture_path == "") { 
		this->TextureID = 0; 
		glGenTextures(1, &this->TextureID);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->TextureID);

		unsigned char whitePixel[] = { 255, 255, 255, 255 };
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		EngineManager::getInstance().SceneChangeEvent();

		return; 
	}

	auto& cache = TextureCache();
	auto it = cache.find(texture_path);
	if (it != cache.end()) {
		it->second.second++;
		this->TextureID = it->second.first;
		EngineManager::getInstance().SceneChangeEvent();
		return;
	}

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true);

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
		cache[texture_path] = { this->TextureID, 1 };
	}
	else {
		Console::PrintError("Failed to load texture");
		glDeleteTextures(1, &this->TextureID);
		this->TextureID = 0;
	}
	stbi_image_free(data);

	EngineManager::getInstance().SceneChangeEvent();
}

std::unordered_map<std::string, std::pair<GLuint, int>>& RenderComponent::TextureCache() {
	static std::unordered_map<std::string, std::pair<GLuint, int>> cache;
	return cache;
}

void RenderComponent::Draw() {
	if (!glResourcesReady) return;  

	this->shader.use();
	if (!Enabled)
		return;

	this->shader.setVec4D("aColor", this->color);

	if (this->TextureID != 0) {
		glActiveTexture(GL_TEXTURE0);
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

				if (abs(area1 + area2 + area3 - areafull) < 0.00000000001) {
					ValidEar = false;
					break;
				}
			}
		}
		if (angle > glm::pi<float>()) {
			ValidEar = false;
			Console::PrintError("Angle is concave, not an ear");
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
				Console::PrintError("No valid ear found, polygon might be malformed");
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
				glm::vec3 p = tc ? tc->ProjectToWorld(worldCorners[i], true, false) : worldCorners[i];
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

				glm::vec3 p = tc ? tc->ProjectToWorld(worldPoint, true, false) : worldPoint;

				float u = (std::cos(theta) + 1.0f) * 0.5f;
				float v = (std::sin(theta) + 1.0f) * 0.5f;
				verts.insert(verts.end(), { p.x, p.y, 0.0f, u, v });
			}
			glm::vec3 centerLocal = tc ? tc->ProjectToWorld(s.center, true, false) : s.center;
			verts.insert(verts.end(), { centerLocal.x, centerLocal.y, 0.0f, 0.5f, 0.5f });

			return verts;
		}
		}, shape);
}

float RenderComponent::GetArea() {
	float totalArea = 0;
	for (int i = 0; i < Indices.size(); i += 3)
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

bool BarycentricWeights(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
	float& u, float& v, float& w) {
	glm::vec3 v0 = b - a, v1 = c - a, v2 = p - a;
	float d00 = glm::dot(v0, v0);
	float d01 = glm::dot(v0, v1);
	float d11 = glm::dot(v1, v1);
	float d20 = glm::dot(v2, v0);
	float d21 = glm::dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	if (std::abs(denom) < 1e-10f) return false; 
	v = (d11 * d20 - d01 * d21) / denom;
	w = (d00 * d21 - d01 * d20) / denom;
	u = 1.0f - v - w;
	return true;
}

glm::vec2 RenderComponent::ComputeUVAtLocalPoint(const glm::vec3& localPoint) {
	glm::vec2 bestUV(0.5f, 0.5f);
	float bestPenalty = INFINITY;

	for (size_t i = 0; i + 2 < Indices.size(); i += 3) {
		unsigned int ia = Indices[i], ib = Indices[i + 1], ic = Indices[i + 2];

		glm::vec3 a(Vertices[ia * 5 + 0], Vertices[ia * 5 + 1], 0.0f);
		glm::vec3 b(Vertices[ib * 5 + 0], Vertices[ib * 5 + 1], 0.0f);
		glm::vec3 c(Vertices[ic * 5 + 0], Vertices[ic * 5 + 1], 0.0f);

		float u, v, w;
		if (!BarycentricWeights(localPoint, a, b, c, u, v, w)) continue;

		float penalty = std::max({ -u, -v, -w, 0.0f });
		if (penalty < bestPenalty) {
			bestPenalty = penalty;
			glm::vec2 uvA(Vertices[ia * 5 + 3], Vertices[ia * 5 + 4]);
			glm::vec2 uvB(Vertices[ib * 5 + 3], Vertices[ib * 5 + 4]);
			glm::vec2 uvC(Vertices[ic * 5 + 3], Vertices[ic * 5 + 4]);
			bestUV = u * uvA + v * uvB + w * uvC;
		}
		if (penalty <= 1e-6f) break; 
	}
	return bestUV;
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

	if (glResourcesReady) {   
		glBindVertexArray(this->VAO);
		glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
		glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(float), Vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(unsigned int), Indices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
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
	target->pendingShape = currentShape;
	target->SetEnabled(Enabled);
}

void RenderComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);

	w.WriteString(texture_path);
	w.Write(color);
	w.Write(z_index);

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
	}
	else {
		w.Write(static_cast<uint8_t>(0));
		auto& s = std::get<PolygonShape>(currentShape);
		w.WriteArray(s.vertices);
	}
}
void RenderComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);

	SetTexture(r.ReadString());
	color = r.Read<glm::vec4>();
	z_index = r.Read<int>();

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
		pendingShape = s;
	}
	else {
		PolygonShape s;
		s.vertices = r.ReadArray<float>();
		pendingShape = s;
	}
}

void RenderComponent::PostLoad() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();

	tc->rotation = tc->pendingRotation;
	tc->size = tc->pendingScale;
	tc->worldMatrixDirty = true;

	std::visit([&](auto&& s) {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape> || std::is_same_v<T, CircleShape>) {
			s.center = tc->GetWorldPosition();
		}
		}, pendingShape);

	SetShape(pendingShape);

	tc->SetRotationCenter(GetCenter());
}

void RenderComponent::ProcessInspectorUI() {
	ImGui::Text("Texture");
	ImGui::SameLine();
	char selected_texture_path[128] = "None (click to choose...)";
	if (!texture_path.empty()) {
		std::string displayPath = FileManager::getInstance().AbsoluteToVirtual(texture_path.c_str());
#if defined(_MSC_VER)
		strcpy_s(selected_texture_path, displayPath.c_str());
#else
		strncpy(selected_texture_path, displayPath.c_str(), sizeof(selected_texture_path) - 1);
#endif
	}
	ImGui::InputText("##Texture path", selected_texture_path, IM_ARRAYSIZE(selected_texture_path), ImGuiInputTextFlags_ReadOnly);
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (ImGui::IsItemClicked()) {
		FileDialogOptions opts;
		opts.title = "Choose Texture";
		opts.filters = {
			{ "Images", "*.png;*.jpeg;*.jpg" },
			{ "All Files", "*.*" }
		};
		if (auto path = FileDialog::ShowOpenDialog(opts)) {
			EditorManager::getInstance().BeginEdit({ parent });
			SetTexture(*path);
			EditorManager::getInstance().EndEdit({ parent });
		}
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FileManager::kResourceDragDropPayloadType)) {
			std::string virtualPath(static_cast<const char*>(payload->Data));
			FileManager& fm = FileManager::getInstance();
			if (!fm.IsDirectory(virtualPath)) {
				EditorManager::getInstance().BeginEdit({ parent });
				SetTexture(fm.VirtualToAbsolute(virtualPath).string());
				EditorManager::getInstance().EndEdit({ parent });
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::Separator();

	float displayColor[4] = { color.x, color.y, color.z, color.a };
	EditorField::ColorEdit4Scene(parent, "Color", "##Color", displayColor, [&] {
		this->color = glm::vec4(displayColor[0], displayColor[1], displayColor[2], displayColor[3]);
		EngineManager::getInstance().SceneChangeEvent();
		});


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
			EditorManager::getInstance().BeginEdit({ parent });

			RectangleShape rect;
			rect.width = rect.height = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			rect.center = tc ? tc->GetWorldPosition() : GetCenter();

			if (Renderer::getInstance().polygonEditGizmos->IsEditing())
				Renderer::getInstance().polygonEditGizmos->EndEdit();
			isAddVertex = false;

			SetShape(rect);

			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("Circle", std::holds_alternative<CircleShape>(currentShape))) {
			EditorManager::getInstance().BeginEdit({ parent });

			CircleShape cir;
			cir.radius = 1.0f;
			TransformComponent* tc = parent->GetComponent<TransformComponent>();
			cir.center = tc ? tc->GetWorldPosition() : GetCenter();

			if (Renderer::getInstance().polygonEditGizmos->IsEditing())
				Renderer::getInstance().polygonEditGizmos->EndEdit();
			isAddVertex = false;

			SetShape(cir);

			EditorManager::getInstance().EndEdit({ parent });
		}
		if (ImGui::Selectable("Polygon", std::holds_alternative<PolygonShape>(currentShape))) {
			EditorManager::getInstance().BeginEdit({ parent });   // NOTE: intentionally NOT ended here — see Polygon session below

			PolygonShape poly;
			poly.vertices = {};

			FluidComponent* fc = parent->GetComponent<FluidComponent>();
			if (fc) {
				fc->ClearParticles();
			}

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
			EditorField::InputFloat2Scene(parent, "  Size", "##RectSize", dims, [&] {
				s.width = std::max(0.01f, dims[0]);
				s.height = std::max(0.01f, dims[1]);
				TransformComponent* tc = parent->GetComponent<TransformComponent>();
				s.center = tc ? tc->GetWorldPosition() : GetCenter();
				SetShape(s);
				}, "%.3f m");
		}
		else if constexpr (std::is_same_v<T, CircleShape>) {
			auto updateCenter = [&]() {
				TransformComponent* tc = parent->GetComponent<TransformComponent>();
				s.center = tc ? tc->GetWorldPosition() : GetCenter();
				};

			float r = s.radius;
			EditorField::InputFloatScene(parent, "  Radius", "##CircleRadius", &r, [&] {
				s.radius = std::max(0.01f, r); updateCenter(); SetShape(s);
				}, "%.3f m");

			int seg = s.segments;
			EditorField::InputIntScene(parent, "  Segments", "##CircleSeg", &seg, [&] {
				s.segments = std::max(3, seg); updateCenter(); SetShape(s);
				});
		}
		else if constexpr (std::is_same_v<T, PolygonShape>) {
			if (!isAddVertex) {
				if (ImGui::Button("Reset vertices##PolyReset"))
				{
					EditorManager::getInstance().BeginEdit({ parent });

					s.vertices = {};

					FluidComponent* fc = parent->GetComponent<FluidComponent>();
					if (fc) fc->particles.clear();
					SetShape(s);

					TransformComponent* tc = parent->GetComponent<TransformComponent>();
					Renderer::getInstance().polygonEditGizmos->BeginEdit(tc, {}, PolygonEditGizmos::VertexAddMode::Append);
					polygonEditCallbackID = Renderer::getInstance().polygonEditGizmos->AddChangeCallback(
						[this](const std::vector<glm::vec3>& verts) { this->ApplyLiveShapeUpdate(verts); });

					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					isAddVertex = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Edit vertices##PolyEditVerts")) {
					EditorManager::getInstance().BeginEdit({ parent });

					std::vector<glm::vec3> localVerts;
					localVerts.reserve(points.size());
					for (auto& p : points) {
						localVerts.push_back(glm::vec3(p[0], p[1], 0.0f));
					}

					TransformComponent* tc = parent->GetComponent<TransformComponent>();
					Renderer::getInstance().polygonEditGizmos->BeginEdit(tc, localVerts, PolygonEditGizmos::VertexAddMode::InsertOnEdge);
					polygonEditCallbackID = Renderer::getInstance().polygonEditGizmos->AddChangeCallback(
						[this](const std::vector<glm::vec3>& verts) { this->ApplyLiveShapeUpdate(verts); });
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::AddVertex);
					isAddVertex = true;
				}
			}
			else {
				using AddMode = PolygonEditGizmos::VertexAddMode;
				AddMode mode = Renderer::getInstance().polygonEditGizmos->GetAddMode();
				const char* helpText =
					(mode == AddMode::Append) ? "Click to add, drag to move, right-click to remove" :
					(mode == AddMode::InsertOnEdge) ? "Click highlighted edge to insert, drag to move, right-click to remove" :
					"Drag to move, right-click to remove";
				const auto& editedVerts = Renderer::getInstance().polygonEditGizmos->GetLocalVertices();
				ImGui::Text("Vertices: %d", (int)editedVerts.size());
				
				ImGui::BeginDisabled(editedVerts.size() < 3);
				if (ImGui::Button("Confirm")) {
					std::vector<float> newVertices;
					newVertices.reserve(editedVerts.size() * 5);
					
					glm::vec3 bmin(INFINITY), bmax(-INFINITY);
					for (auto& v : editedVerts) { bmin = glm::min(bmin, v); bmax = glm::max(bmax, v); }
					glm::vec3 range = glm::max(bmax - bmin, glm::vec3(1e-6f));

					for (auto& v : editedVerts) {
						float u = (v.x - bmin.x) / range.x;
						float uvY = (v.y - bmin.y) / range.y;
						newVertices.insert(newVertices.end(), { v.x, v.y, 0.0f, u, uvY });
					}
					
					s.vertices = newVertices;
					SetShape(s);
					
					SoftBodyComponent * sb = parent->GetComponent<SoftBodyComponent>();
					if (sb) sb->RebuildMassAggregate();
			
					FluidComponent * fc = parent->GetComponent<FluidComponent>();
					if (fc) {
						fc->SeedParticles();
						fc->ResizeInstanceBuffer();
					}
					
					Renderer::getInstance().polygonEditGizmos->EndEdit();
					Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(polygonEditCallbackID);
					polygonEditCallbackID = -1;
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
					isAddVertex = false;

					EditorManager::getInstance().EndEdit({ parent });
				}
				ImGui::EndDisabled();
				
				ImGui::SameLine();
				if (ImGui::Button("Cancel##PolyCancel")) {
					Renderer::getInstance().polygonEditGizmos->EndEdit();
					Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(polygonEditCallbackID);
					polygonEditCallbackID = -1;
					EngineManager::getInstance().SwitchInteractMode(EngineManager::InteractMode::EditorSelect);
					isAddVertex = false;

					EditorManager::getInstance().EndEdit({ parent });
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

	EditorField::InputIntScene(parent, "Z index", "##ZIndex", &z_index, [] {
		EngineManager::getInstance().SceneChangeEvent();
		});
}

void RenderComponent::OnDelete() {
	if (physicsChangeEventCallbackID != -1) {
		EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsChangeEventCallbackID);
		physicsChangeEventCallbackID = -1;
	}
	if (polygonEditCallbackID != -1) {
		Renderer::getInstance().polygonEditGizmos->RemoveChangeCallback(polygonEditCallbackID);
		polygonEditCallbackID = -1;
	}

	if (!glResourcesReady) return;   

	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteVertexArrays(1, &VAO);

	if (texture_path != "" && TextureID != 0) {
		auto& cache = TextureCache();
		auto it = cache.find(texture_path);
		if (it != cache.end()) {
			if (--it->second.second <= 0) {
				glDeleteTextures(1, &it->second.first);
				cache.erase(it);
			}
		}
	}
}