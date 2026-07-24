#include "../../Header Files/Core/Camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/io.hpp>

void Camera::Setup() {
	InputManager::getInstance().SetMouseScrollCallback([this](double xoffset, double yoffset) {this->ScrollCallback(xoffset, yoffset);}, 999);
	viewMatrix = glm::mat4(1);
	viewMatrixInverse = glm::mat4(1);
	viewMatrix = glm::translate(viewMatrix, -cameraPos);
	glm::mat4 inverseTranslate = glm::translate(glm::mat4(1.0f), cameraPos);
	viewMatrixInverse = inverseTranslate * viewMatrixInverse;
	viewMatrix = glm::rotate(viewMatrix, -cameraRotation, glm::vec3(0, 0, 1));
	glm::mat4 inverseRotate = glm::rotate(glm::mat4(1.0f), cameraRotation, glm::vec3(0, 0, 1));
	viewMatrixInverse = inverseRotate * viewMatrixInverse;
	viewMatrix = glm::scale(viewMatrix, glm::vec3(1.0f / cameraZoom, 1.0f / cameraZoom, 1));
	//inversing multiplication order as well when inversing matrix
	glm::mat4 inverseScale = glm::scale(glm::mat4(1.0f), glm::vec3(cameraZoom, cameraZoom, 1.0f));
	viewMatrixInverse = inverseScale * viewMatrixInverse;
}

void Camera::ProcessCamera(float delta) {
	this->delta = delta;

	if (InputManager::keys.contains(GLFW_KEY_W) && InputManager::keys[GLFW_KEY_W] && !InputManager::keys[GLFW_KEY_LEFT_CONTROL]) {
		SetCameraPosition(glm::vec3(cameraPos.x, cameraPos.y + cameraSpeed * delta, cameraPos.z));
	}
	if (InputManager::keys.contains(GLFW_KEY_A) && InputManager::keys[GLFW_KEY_A] && !InputManager::keys[GLFW_KEY_LEFT_CONTROL]) {
		SetCameraPosition(glm::vec3(cameraPos.x - cameraSpeed * delta, cameraPos.y, cameraPos.z));
	}
	if (InputManager::keys.contains(GLFW_KEY_S) && InputManager::keys[GLFW_KEY_S] && !InputManager::keys[GLFW_KEY_LEFT_CONTROL]) {
		SetCameraPosition(glm::vec3(cameraPos.x, cameraPos.y - cameraSpeed * delta, cameraPos.z));
	}
	if (InputManager::keys.contains(GLFW_KEY_D) && InputManager::keys[GLFW_KEY_D] && !InputManager::keys[GLFW_KEY_LEFT_CONTROL]) {
		SetCameraPosition(glm::vec3(cameraPos.x + cameraSpeed * delta, cameraPos.y, cameraPos.z));
	}

	viewMatrix = glm::mat4(1);
	viewMatrixInverse = glm::mat4(1);
	viewMatrix = glm::translate(viewMatrix, -cameraPos);
	glm::mat4 inverseTranslate = glm::translate(glm::mat4(1.0f), cameraPos);
	viewMatrixInverse = inverseTranslate * viewMatrixInverse;
	viewMatrix = glm::rotate(viewMatrix, -cameraRotation, glm::vec3(0, 0, 1));
	glm::mat4 inverseRotate = glm::rotate(glm::mat4(1.0f), cameraRotation, glm::vec3(0, 0, 1));
	viewMatrixInverse = inverseRotate * viewMatrixInverse;
	viewMatrix = glm::scale(viewMatrix, glm::vec3(1.0f / cameraZoom, 1.0f / cameraZoom, 1));
	//inversing multiplication order as well when inversing matrix
	glm::mat4 inverseScale = glm::scale(glm::mat4(1.0f), glm::vec3(cameraZoom, cameraZoom, 1.0f));
	viewMatrixInverse = inverseScale * viewMatrixInverse;
}

void Camera::SetCameraPosition(glm::vec3 pos) {
	this->cameraPos = pos;
}

void Camera::SetCameraRotation(float rot) {
	this->cameraRotation = rot;
}

void Camera::SetCameraZoom(float zoom) {
	this->cameraZoom = zoom;
}

void Camera::ScrollCallback(double xoffset, double yoffset) {
	float fov = cameraZoom - yoffset;
	if (fov < 1) {
		fov = 1;
	}
	else if (fov > 1000) {
		fov = 1000;
	}
	SetCameraZoom(fov);
}