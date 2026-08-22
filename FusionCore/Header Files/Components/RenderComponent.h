#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Component.h"
#include "../Core/Rendering/Shader.h" 
#include "../../stb_image.h"
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <variant>
#include <array>
#include "TransformComponent.h"
#include "../Core/Files/FileDialog.h"
#include "../Core/Rendering/Shapes.h"

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

	bool glResourcesReady = false;

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);
	virtual void PostLoad();
	
	void EnsureGLResources();

	int AddOnShapeSetCallback(std::function<void()> func);
	void RemoveOnShapeSetCallback(int ID);

	std::vector<float> VerticesFromShape(Shape& shape);
	std::vector<unsigned int> TriangulateCircle(int segments);
	std::vector<unsigned int> Triangulate(std::vector<float> vertices);
	float GetArea();
	glm::vec3 GetCenter();
	glm::vec2 ComputeUVAtLocalPoint(const glm::vec3& localPoint);
	void SetShape(Shape shape);
	void SetTexture(std::string texture_path);
	bool IsInsideShape(glm::vec3 point);
	void UpdateShape(std::vector<float> vertices, std::vector<unsigned int> indices);
	void Draw();

private:
	int physicsChangeEventCallbackID = -1;

	void ApplyLiveShapeUpdate(const std::vector<glm::vec3>& verts);
	int polygonEditCallbackID = -1;

	static std::unordered_map<std::string, std::pair<GLuint, int>>& TextureCache();

	std::unordered_map<int, std::function<void()>> OnShapeSetCallbacks;
	int shapeCallbackID = -1;

	bool initialized = false;
	Shader shader;
	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;
	unsigned int TextureID;
};

