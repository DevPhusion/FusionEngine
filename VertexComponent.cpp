#include "VertexComponent.h"
#include "ObjectManager.h"

bool VertexComponent::vertexSelected = false;

VertexComponent::VertexComponent(Object* parent) : ComponentBase<VertexComponent>(parent) {
	Name = "Vertex Component";
	Hidden = true;
}

void VertexComponent::SetEnabled(bool enabled) {
	Enabled = enabled;

	if (!enabled) {
		selectedIndex = -1;
	}

	for (int i = 0; i < vertexPoints.size(); i++)
	{
		vertexPoints[i]->GetComponent<RenderComponent>()->SetEnabled(enabled);
	}
}

void VertexComponent::RemoveAllVertex() {
	for (int i = 0; i < vertexPoints.size(); i++)
	{
		ObjectManager::getInstance().RemoveObject(vertexPoints[i]);
	}

	vertexPoints.clear();
}

void VertexComponent::CopyTo(Object* other) {
	VertexComponent* target = other->GetComponent<VertexComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<VertexComponent>(other));
		target = other->GetComponent<VertexComponent>();
	}

	std::vector<VertexPoint*> vPoints;

	for (int i = 0; i < vertexPoints.size(); i++)
	{
		VertexPoint* v = ObjectManager::getInstance().CopyVertex(vertexPoints[i]);
		if (v) {
			vPoints.push_back(v);
		}
	}

	target->SetVertexPoints(vPoints);
	target->UpdateTransform();
}

void VertexComponent::ProcessInspectorUI() {
	if (!this->parent->GetComponent<TransformComponent>()->Enabled) {
		SetEnabled(false);
	}
}

void VertexComponent::OnDelete() {
	InputManager::getInstance().RemoveMouseButtonCallback(mouseButtonCallbackID);
	InputManager::getInstance().RemoveCursorPositionCallback(cursorPosCallbackID);
}

int VertexComponent::GetSelectedVertex() {
	for (int i = 0; i < vertexPoints.size(); i++)
	{
		glm::vec3 center = vertexPoints[i]->GetComponent<TransformComponent>()->GetTransformedPoint(glm::vec3(vertexPoints[i]->x, vertexPoints[i]->y, 0));

		float distance = sqrt(pow(InputManager::glX - center.x, 2) + pow(InputManager::glY - center.y, 2));

		if (distance < 0.05f && !vertexSelected) {
			return i;
		}
	}

	return -1;
}

void VertexComponent::SetVertexPoints(std::vector<VertexPoint*> vertexPoints) {
	this->vertexPoints =  std::move(vertexPoints);
	mouseButtonCallbackID = InputManager::getInstance().SetMouseButtonCallback([this](int button, int action, int mods) { this->FindSelectedPoint(button, action, mods); }, 0);
	cursorPosCallbackID = InputManager::getInstance().SetCursorPositionCallback([this](double xpos, double ypos) { this->DragPoint(xpos, ypos); }, 0);
	for (int i = 0; i < this->vertexPoints.size(); i++)
	{
		this->vertexPoints[i]->GetComponent<TransformComponent>()->SetEnabled(true);
	}

	this->parent->GetComponent<TransformComponent>()->AddTransformCallback([this] { this->UpdateTransform(); });
	SetEnabled(EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::EditorSelect);
}

void VertexComponent::FindSelectedPoint(int button, int action, int mods) {
	if (!Enabled) {
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		int index = GetSelectedVertex();
		if (index != -1) {
			vertexSelected = true;
			selectedIndex = index;
		}
	}
}

void VertexComponent::DragPoint(double xpos, double ypos) {
	if (InputManager::mouseLeftHold) {
		if (selectedIndex != -1 ) {
			glm::vec3 pos = vertexPoints[selectedIndex]->GetComponent<TransformComponent>()->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);

			std::vector<float> newVertices = parent->GetComponent<RenderComponent>()->Vertices;
			newVertices[selectedIndex * 5] = pos.x;
			newVertices[selectedIndex * 5 + 1] = pos.y;
			
			RenderComponent* render = parent->GetComponent<RenderComponent>();
			PolygonShape poly = PolygonShape();
			poly.vertices = newVertices;
			render->SetShape(poly);
			parent->GetComponent<TransformComponent>()->SetRotationCenter(render->GetCenter());

			vertexPoints[selectedIndex]->UpdatePosition(pos.x, pos.y);
			this->parent->GetComponent<TransformComponent>()->UpdateWorldPosition(this->parent->GetComponent<TransformComponent>()->GetWorldPosition());
			RigidBodyComponent* pc = this->parent->GetComponent<RigidBodyComponent>();
			if (pc) {
				pc->CalculateInertia();
			}
			SoftBodyComponent* sb = this->parent->GetComponent<SoftBodyComponent>();
			if (sb) {
				sb->RebuildMassAggregate();
			}
		}
	}
	else {
		vertexSelected = false;
		selectedIndex = -1;
	}
}

void VertexComponent::UpdateTransform() {
	TransformComponent* parentTransform = this->parent->GetComponent<TransformComponent>();

	for (int i = 0; i < vertexPoints.size(); i++)
	{
		TransformComponent* vertexTransform = vertexPoints[i]->GetComponent<TransformComponent>();
		vertexTransform->SetOriginTransform(parentTransform->OriginTransform);
		vertexTransform->SetRotationCenter(parentTransform->rotation_center);
		vertexTransform->Translate(parentTransform->position);
		vertexTransform->Rotate(parentTransform->rotation);
		vertexTransform->Scale(parentTransform->size);
		vertexTransform->UpdateWorldPosition(vertexTransform->GetWorldPosition());
	}
}