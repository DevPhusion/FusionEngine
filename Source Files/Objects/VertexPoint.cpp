#include "../../Header Files/Objects/VertexPoint.h"

VertexPoint::VertexPoint(float x, float y, Shader shader) : Object(shader) {
	this->x = x;
	this->y = y;

	this->hidden = true;

	float sizeY = 0.01f;
	float sizeX = 0.01f;

	std::vector<float> vertices = {
		x - sizeX, y - sizeY, 0.0f, 0.0f, 0.0f,
		x + sizeX, y - sizeY, 0.0f, 1.0f, 0.0f,
		x + sizeX, y + sizeY, 0.0f, 1.0f, 1.0f,
		x - sizeX, y + sizeY, 0.0f, 0.0f, 1.0f
	};

	AddComponent(std::make_unique<RenderComponent>(this, vertices, shader, ""));
	AddComponent(std::make_unique<TransformComponent>(this, shader, glm::vec3(0)));
}

void VertexPoint::Process(float delta) {
	
}

std::unique_ptr<VertexPoint> VertexPoint::CloneVertex() {
	std::unique_ptr<VertexPoint> newVert = std::make_unique<VertexPoint>(x, y, Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
	newVert->UpdatePosition(x, y);
	newVert->id = id;
	return newVert;
}

void VertexPoint::SerializeVertex(BinaryWriter& w) {
	Serialize(w);

	w.Write(x);
	w.Write(y);
}

void VertexPoint::DeserializeVertex(BinaryReader& r) {
	Deserialize(r);

	x = r.Read<float>();
	y = r.Read<float>();
	UpdatePosition(x, y);

	float sizeY = 0.01f;
	float sizeX = 0.01f;

	std::vector<float> vertices = {
		x - sizeX, y - sizeY, 0.0f, 0.0f, 0.0f,
		x + sizeX, y - sizeY, 0.0f, 1.0f, 0.0f,
		x + sizeX, y + sizeY, 0.0f, 1.0f, 1.0f,
		x - sizeX, y + sizeY, 0.0f, 0.0f, 1.0f
	};

	GetComponent<RenderComponent>()->UpdateShape(vertices, GetComponent<RenderComponent>()->Triangulate(vertices));
}

void VertexPoint::UpdatePosition(float x, float y) {
	this->x = x;
	this->y = y;

	float sizeY = 0.01f;
	float sizeX = 0.01f;

	std::vector<float> vertices = {
		x - sizeX, y - sizeY, 0.0f, 0.0f, 0.0f,
		x + sizeX, y - sizeY, 0.0f, 1.0f, 0.0f,
		x + sizeX, y + sizeY, 0.0f, 1.0f, 1.0f,
		x - sizeX, y + sizeY, 0.0f, 0.0f, 1.0f
	};

	GetComponent<RenderComponent>()->UpdateShape(vertices, GetComponent<RenderComponent>()->Indices);
}