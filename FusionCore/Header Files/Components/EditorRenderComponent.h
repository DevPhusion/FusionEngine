#pragma once
#include "Component.h"
#include "../Objects/Object.h"

struct Edge;

class EditorRenderComponent : public ComponentBase<EditorRenderComponent>
{
public:
	EditorRenderComponent(Object* parent, Shader shader, std::string texture_path = "", float halfSize = 0.15f);
	EditorRenderComponent() = default;

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	void Draw();
	void SetTexture(std::string texture_path);
	glm::vec3 GetCenter();
	bool IsInsideShape(glm::vec3 point);
	void RebuildEdges();

	std::vector<Edge> edges;
	glm::vec4 color = glm::vec4(1, 1, 1, 1);
	int z_index = 999;
	float halfSize = 0.15f;
	std::string texture_path;

private:
	static std::unordered_map<std::string, std::pair<GLuint, int>>& TextureCache();

	Shader shader;
	std::vector<float> Vertices;
	std::vector<unsigned int> Indices;
	GLuint VAO = 0, VBO = 0, EBO = 0;
	GLuint TextureID = 0;
};