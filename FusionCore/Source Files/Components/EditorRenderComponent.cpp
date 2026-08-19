#include "../../Header Files/Components/EditorRenderComponent.h"
#include "../../Header Files/Components/TransformComponent.h"

EditorRenderComponent::EditorRenderComponent(Object* parent, Shader shader, std::string texture_path, float halfSize)
	: ComponentBase<EditorRenderComponent>(parent) {
	Name = "Editor Render Component";
	CanRemove = false;
	CanDisable = false;
	Hidden = true;
	this->shader = shader;
	this->halfSize = halfSize;

	bool headless = EngineManager::getInstance().isHeadless;

	Vertices = {
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
	};
	Indices = { 0, 1, 2, 2, 3, 0 };

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

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	SetTexture(texture_path);

	if (parent->HasComponent<TransformComponent>()) {
		parent->GetComponent<TransformComponent>()->AddTransformCallback([this]() { RebuildEdges(); });
	}
	RebuildEdges();
}

void EditorRenderComponent::SetTexture(std::string texture_path) {
	if (EngineManager::getInstance().isHeadless) {
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

std::unordered_map<std::string, std::pair<GLuint, int>>& EditorRenderComponent::TextureCache() {
	static std::unordered_map<std::string, std::pair<GLuint, int>> cache;
	return cache;
}

void EditorRenderComponent::Draw() {
	if (!Enabled || EngineManager::getInstance().isHeadless)
		return;

	this->shader.use();
	this->shader.setVec4D("aColor", this->color);

	if (this->TextureID != 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->TextureID);
	}

	glm::vec3 center = GetCenter();

	float zoom = Camera::getInstance().cameraZoom;
	if (zoom < 1e-6f) zoom = 1.0f;

	glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
	model = glm::scale(model, glm::vec3(halfSize * zoom, halfSize * zoom, 1.0f));

	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio,
		EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);

	this->shader.setMat4D("projection", projection);
	this->shader.setMat4D("view", Camera::getInstance().viewMatrix);
	this->shader.setMat4D("transform", model);

	glBindVertexArray(this->VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
	glDrawElements(GL_TRIANGLES, Indices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

glm::vec3 EditorRenderComponent::GetCenter() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	return tc ? tc->GetWorldPosition() : glm::vec3(0);
}

void EditorRenderComponent::RebuildEdges() {
	glm::vec3 c = GetCenter();
	glm::vec3 corners[4] = {
		c + glm::vec3(-halfSize, -halfSize, 0),
		c + glm::vec3(halfSize, -halfSize, 0),
		c + glm::vec3(halfSize,  halfSize, 0),
		c + glm::vec3(-halfSize,  halfSize, 0),
	};
	edges.clear();
	for (int i = 0; i < 4; i++) {
		Edge e;
		e.start = corners[i];
		e.end = corners[(i + 1) % 4];
		edges.push_back(e);
	}
}

bool EditorRenderComponent::IsInsideShape(glm::vec3 point) {
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

void EditorRenderComponent::CopyTo(Object* other) {
	EditorRenderComponent* target = other->GetComponent<EditorRenderComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<EditorRenderComponent>(other, other->shader, texture_path, halfSize));
		target = other->GetComponent<EditorRenderComponent>();
	}

	target->z_index = z_index;
	target->SetTexture(texture_path);
	target->color = color;
	target->SetEnabled(Enabled);
}

void EditorRenderComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.WriteString(texture_path);
	w.Write(color);
	w.Write(z_index);
	w.Write(halfSize);
}
void EditorRenderComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	SetTexture(r.ReadString());
	color = r.Read<glm::vec4>();
	z_index = r.Read<int>();
	halfSize = r.Read<float>();
}

void EditorRenderComponent::ProcessInspectorUI() {
	
}

void EditorRenderComponent::OnDelete() {
	if (EngineManager::getInstance().isHeadless) return;

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