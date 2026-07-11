#include "Gizmos.h"
#include "EditorManager.h"
#include "Renderer.h"

Gizmos::Gizmos() {
    mouseButtonCallbackID = InputManager::getInstance().SetMouseButtonCallback(
        [this](int button, int action, int mods) {OnMouseButton(button, action, mods);}, 999);
    cursorPosCallbackID = InputManager::getInstance().SetCursorPositionCallback(
        [this](double xpos, double ypos) {OnCursorPosition(xpos, ypos);}, 999);
}

Gizmos::~Gizmos() {
    if (isInitialized) {
        glDeleteVertexArrays(1, &gizmoVAO);
        glDeleteBuffers(1, &gizmoVBO);
    }
}

void Gizmos::Initialize() {
    if (isInitialized) return;

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &gizmoVAO);
    glGenBuffers(1, &gizmoVBO);
    glBindVertexArray(gizmoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    isInitialized = true;
}

void Gizmos::DrawFilledQuad(glm::vec3 center, glm::vec3 size, glm::vec4 color, bool screenSpace) {
    if (!isInitialized) Initialize();

    glm::vec3 renderSize = size;
    if (screenSpace) {
        float zoom = Camera::getInstance().cameraZoom;
        if (zoom > 1e-6f) {
            renderSize = glm::vec3(size.x * zoom, size.y * zoom, size.z);
        }
    }

    Shader lineShader("vertex.txt", "fragment.txt");
    lineShader.use();
    lineShader.setVec4D("aColor", color);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, renderSize);

    glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio,
        EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);

    lineShader.setMat4D("projection", projection);
    lineShader.setMat4D("view", Camera::getInstance().viewMatrix);
    lineShader.setMat4D("transform", model);

    glBindVertexArray(gizmoVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

TransformComponent* Gizmos::GetSelectedTransform() {
    Object* selected = EditorManager::getInstance().selectedObject;
    if (!selected) return nullptr;
    if (!selected->HasComponent<TransformComponent>()) return nullptr;

    return selected->GetComponent<TransformComponent>();
}

void Gizmos::UpdateGizmos() {
    if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate)
        return;

    switch (currentGizmosMode) {
    case GizmosMode::Move:
        DrawMoveGizmos();
        break;
    case GizmosMode::Rotate:
        DrawRotateGizmos();
        break;
    case GizmosMode::Scale:
        DrawScaleGizmos();
        break;
    }
}

glm::vec3 Gizmos::ScreenToWorld(double xpos, double ypos) {
	float windowWidth = (float)EngineManager::getInstance().windowWidth;
	float windowHeight = (float)EngineManager::getInstance().windowHeight;
	float aspectRatio = EngineManager::getInstance().aspectRatio;

	float ndcX = ((float)xpos / windowWidth) * 2.0f - 1.0f;
	float ndcY = 1.0f - ((float)ypos / windowHeight) * 2.0f; 

	float viewX = ndcX * aspectRatio; 
	float viewY = ndcY;

	glm::vec4 viewPos = glm::vec4(viewX, viewY, 0.0f, 1.0f);
	glm::vec4 worldPos = Camera::getInstance().viewMatrixInverse * viewPos;

	return glm::vec3(worldPos.x, worldPos.y, 0.0f);
}

float Gizmos::DistancePointToSegment(glm::vec3 p, glm::vec3 a, glm::vec3 b) {
	glm::vec2 p2 = glm::vec2(p.x, p.y);
	glm::vec2 a2 = glm::vec2(a.x, a.y);
	glm::vec2 b2 = glm::vec2(b.x, b.y);

	glm::vec2 ab = b2 - a2;
	float lengthSq = glm::dot(ab, ab);

	float t = 0.0f;
	if (lengthSq > 1e-8f) {
		t = glm::dot(p2 - a2, ab) / lengthSq;
		t = glm::clamp(t, 0.0f, 1.0f);
	}

	glm::vec2 closest = a2 + ab * t;
	return glm::length(p2 - closest);
}

float Gizmos::GetAngle(glm::vec3 origin, glm::vec3 point) {
	glm::vec2 d = glm::vec2(point.x - origin.x, point.y - origin.y);
	return atan2(d.y, d.x);
}

GizmosAxis Gizmos::HitTestMoveScale(glm::vec3 origin) {
	float zoom = Camera::getInstance().cameraZoom;
	if (zoom < 1e-6f) zoom = 1.0f;

	glm::vec3 tipX = origin + glm::vec3(1.0f, 0.0f, 0.0f) * axisLength * zoom;
	glm::vec3 tipY = origin + glm::vec3(0.0f, 1.0f, 0.0f) * axisLength * zoom;

	float threshold = hitPadding * zoom;

	float distX = DistancePointToSegment(currentMouseWorld, origin, tipX);
	float distY = DistancePointToSegment(currentMouseWorld, origin, tipY);

	if (distX <= threshold && distX <= distY) return GizmosAxis::X;
	if (distY <= threshold) return GizmosAxis::Y;

	return GizmosAxis::None;
}

bool Gizmos::HitTestCircle(glm::vec3 origin) {
	float zoom = Camera::getInstance().cameraZoom;
	if (zoom < 1e-6f) zoom = 1.0f;

	float radius = circleRadius * zoom;
	float dist = glm::length(glm::vec2(currentMouseWorld.x, currentMouseWorld.y) - glm::vec2(origin.x, origin.y));

	float ringThickness = hitPadding * zoom;
	return std::fabs(dist - radius) <= ringThickness;
}

void Gizmos::OnMouseButton(int button, int action, int mods) {
	if (button != GLFW_MOUSE_BUTTON_LEFT) return;
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;

	if (action == GLFW_PRESS) {
		TransformComponent* transform = GetSelectedTransform();
		if (!transform) return;

		glm::vec3 origin = transform->GetWorldPosition();

		if (currentGizmosMode == GizmosMode::Move || currentGizmosMode == GizmosMode::Scale) {
			selectedAxis = HitTestMoveScale(origin);
		}
		else if (currentGizmosMode == GizmosMode::Rotate) {
			selectedAxis = HitTestCircle(origin) ? GizmosAxis::Rotate : GizmosAxis::None;
		}

		if (selectedAxis == GizmosAxis::None) return;

		isDragging = true;
		dragStartMouseWorld = currentMouseWorld;
		dragStartObjectPosition = origin;
		dragStartObjectScale = transform->size;
		dragStartObjectRotation = transform->rotation;
		dragStartAngleOffset = GetAngle(origin, currentMouseWorld);
	}
	else if (action == GLFW_RELEASE) {
		isDragging = false;
		selectedAxis = GizmosAxis::None;
	}
}

void Gizmos::OnCursorPosition(double xpos, double ypos) {
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;
	currentMouseWorld = ScreenToWorld(xpos, ypos);

	if (!isDragging || selectedAxis == GizmosAxis::None) return;

	TransformComponent* transform = GetSelectedTransform();
	if (!transform) return;

	if (currentGizmosMode == GizmosMode::Move && (selectedAxis == GizmosAxis::X || selectedAxis == GizmosAxis::Y)) {
		glm::vec3 axisDir = (selectedAxis == GizmosAxis::X) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		glm::vec3 delta = currentMouseWorld - dragStartMouseWorld;
		float projected = glm::dot(glm::vec2(delta.x, delta.y), glm::vec2(axisDir.x, axisDir.y));

		glm::vec3 newWorldPos = dragStartObjectPosition + axisDir * projected;
		transform->UpdateWorldPosition(newWorldPos);
	}
	else if (currentGizmosMode == GizmosMode::Scale && (selectedAxis == GizmosAxis::X || selectedAxis == GizmosAxis::Y)) {
		glm::vec3 axisDir = (selectedAxis == GizmosAxis::X) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		glm::vec3 delta = currentMouseWorld - dragStartMouseWorld;
		float projected = glm::dot(glm::vec2(delta.x, delta.y), glm::vec2(axisDir.x, axisDir.y));

		glm::vec3 newScale = dragStartObjectScale;
		if (selectedAxis == GizmosAxis::X) newScale.x = glm::max(0.01f, dragStartObjectScale.x + projected);
		else newScale.y = glm::max(0.01f, dragStartObjectScale.y + projected);

		transform->Scale(newScale);
	}
	else if (currentGizmosMode == GizmosMode::Rotate && selectedAxis == GizmosAxis::Rotate) {
		glm::vec3 origin = transform->GetWorldPosition();
		float currentAngle = GetAngle(origin, currentMouseWorld);
		float angleDelta = currentAngle - dragStartAngleOffset;

		transform->Rotate(dragStartObjectRotation + angleDelta);
	}
}

void Gizmos::DrawMoveGizmos() {
    TransformComponent* transform = GetSelectedTransform();
    if (!transform) return;

    glm::vec3 origin = transform->GetWorldPosition();

    Renderer::getInstance().DrawArrow(origin, glm::vec3(1.0f, 0.0f, 0.0f), axisLength, xAxisColor, 5.0f, 0.05f, 30.0f, true);
    Renderer::getInstance().DrawArrow(origin, glm::vec3(0.0f, 1.0f, 0.0f), axisLength, yAxisColor, 5.0f, 0.05f, 30.0f, true);
}

void Gizmos::DrawRotateGizmos() {
    TransformComponent* transform = GetSelectedTransform();
    if (!transform) return;

    glm::vec3 origin = transform->GetWorldPosition();

    Renderer::getInstance().DrawCircle(origin, circleRadius, rotateColor, 32, 5.0f, true);
}

void Gizmos::DrawScaleGizmos() {
    TransformComponent* transform = GetSelectedTransform();
    if (!transform) return;

    glm::vec3 origin = transform->GetWorldPosition();

    DrawSquareArrow(origin, glm::vec3(1.0f, 0.0f, 0.0f), axisLength, xAxisColor, 5.0f, 0.025f, true);
    DrawSquareArrow(origin, glm::vec3(0.0f, 1.0f, 0.0f), axisLength, yAxisColor, 5.0f, 0.025f, true);
}

void Gizmos::DrawSquareArrow(glm::vec3 origin, glm::vec3 direction, float length, glm::vec4 color, float thickness, float squareSize, bool screenSpace) {
    if (glm::length(direction) < 1e-8f) return;

    float zoom = 1.0f;
    if (screenSpace) {
        zoom = Camera::getInstance().cameraZoom;
        if (zoom < 1e-6f) zoom = 1.0f;
    }

    glm::vec3 dir = glm::normalize(direction);
    glm::vec3 tip = origin + dir * length * zoom;

    Renderer::getInstance().DrawLine(origin, tip, color, thickness);

    DrawFilledQuad(tip, glm::vec3(squareSize, squareSize, 1.0f), color, screenSpace);
}