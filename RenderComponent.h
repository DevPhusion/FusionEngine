#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Component.h"
#include "Shader.h" 
#include "stb_image.h"
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <variant>
#include "imgui/ImGuiFileDialog.h"
#include "TransformComponent.h"

struct Edge {
	glm::vec3 start;
	glm::vec3 end;
};


struct PolygonShape {
	std::vector<float> vertices;
};

struct RectangleShape {
	glm::vec3 center;
	float width;
	float height;
};

struct CircleShape {
	glm::vec3 center;
	float radius;
	int segments = 30;
	int physicsSegments = 30;
};

using Shape = std::variant<PolygonShape, RectangleShape, CircleShape>;

class RenderComponent: public ComponentBase<RenderComponent>
{
public:
	RenderComponent(Object* parent, std::vector<float> vertices, Shader shader, std::string texture_path);
	RenderComponent() = default;

	Shape currentShape;
	Shape pendingShape;
	
	std::vector<float> Vertices;
	std::vector<unsigned int> Indices;
	std::vector<std::vector<float>> points;
	std::vector<Edge> edges;
	std::string texture_path;
	glm::vec4 color = glm::vec4(1.0f);
	int z_index; // ordering when drawing

	bool isAddVertex = false; // only for polygon (for adding vertex when doing reset shape)

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	int AddOnShapeSetCallback(std::function<void()> func);
	void RemoveOnShapeSetCallback(int ID);

	std::vector<float> VerticesFromShape(Shape& shape);
	std::vector<unsigned int> TriangulateCircle(int segments);
	std::vector<unsigned int> Triangulate(std::vector<float> vertices);
	float GetArea();
	glm::vec3 GetCenter();
	void SetShape(Shape shape);
	void SetTexture(std::string texture_path);
	bool IsInsideShape(glm::vec3 point);
	void UpdateShape(std::vector<float> vertices, std::vector<unsigned int> indices);
	void Draw();

private:

	std::unordered_map<int, std::function<void()>> OnShapeSetCallbacks;
	int shapeCallbackID = -1;

	bool initialized = false;
	Shader shader;
	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;
	unsigned int TextureID;
};

