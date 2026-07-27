#pragma once
#include "InputManager.h"
#include <glm/glm.hpp>

class Camera
{
public:
	Camera(const Camera&) = delete;
	void operator=(const Camera&) = delete;

	static Camera& getInstance() {
		static Camera instance;
		return instance;
	}

	glm::mat4 viewMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrixInverse = glm::mat4(1.0f);
	glm::vec3 cameraPos = glm::vec3(0);
	float cameraSpeed = 2.0f;
	float cameraRotation = 0.0f;
	float cameraZoom = 5;
	float delta = 0;

	void Setup();
	void ProcessCamera(float delta);
	void SetCameraPosition(glm::vec3 pos);
	void SetCameraRotation(float rot);
	void SetCameraZoom(float zoom);
	void ScrollCallback(double xoffset, double yoffset);
private:
	Camera() {};
};

