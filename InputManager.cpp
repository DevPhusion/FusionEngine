#include "InputManager.h"

float InputManager::glX = 0.0f;
float InputManager::glY = 0.0f;
bool InputManager::mouseLeftHold = false;
bool InputManager::mouseRightHold = false;

std::unordered_map<std::vector<int>, std::function<void(int, int, int)>, VectorHasher> InputManager::MouseButtonCalls = {};
std::unordered_map <std::vector<int>, std::function<void(double, double) >, VectorHasher> InputManager::CursorPositionCalls = {};
std::unordered_map <std::vector<int>, std::function<void(int, int, int, int)>, VectorHasher> InputManager::KeyButtonCalls = {};
std::unordered_map <std::vector<int>, std::function<void(double, double)>, VectorHasher> InputManager::MouseScrollCalls = {};

std::unordered_map<int, bool> InputManager::keys = {};

void InputManager::Setup(GLFWwindow* window) {
	this->window = window;
	glfwSetMouseButtonCallback(this->window, OnMouseButton);
	glfwSetCursorPosCallback(this->window, OnCursorPosition);
	glfwSetKeyCallback(this->window, OnKeyButton);
	glfwSetScrollCallback(this->window, OnMouseScroll);

	glfwSetCharCallback(this->window, ImGui_ImplGlfw_CharCallback);
}

std::vector<int> InputManager::SetMouseButtonCallback(std::function<void(int, int, int)> func, int priorityIndex) {
	CurrentMouseButtonID += 1;
	std::vector<int> ID = { CurrentMouseButtonID, priorityIndex };
	MouseButtonCalls[ID] = func;
	return ID;
}

std::vector<int> InputManager::SetCursorPositionCallback(std::function<void(double, double)> func, int priorityIndex) {
	CurrentCursorPositionID += 1;
	std::vector<int> ID = { CurrentCursorPositionID, priorityIndex };
	CursorPositionCalls[ID] = func;
	return ID;
}

std::vector<int> InputManager::SetKeyButtonCallback(std::function<void(int, int, int, int)> func, int priorityIndex) {
	CurrentKeyButtonID += 1;
	std::vector<int> ID = { CurrentKeyButtonID, priorityIndex };
	KeyButtonCalls[ID] = func;
	return ID;
}

std::vector<int> InputManager::SetMouseScrollCallback(std::function<void(double, double)> func, int priorityIndex) {
	CurrentMouseScrollID += 1;
	std::vector<int> ID = { CurrentMouseScrollID, priorityIndex };
	MouseScrollCalls[ID] = func;
	return ID;
}

void InputManager::RemoveMouseButtonCallback(std::vector<int> ID) {
	MouseButtonCalls.erase(ID);
}

void InputManager::RemoveCursorPositionCallback(std::vector<int> ID) {
	CursorPositionCalls.erase(ID);
}

void InputManager::RemoveKeyButtonCallback(std::vector<int> ID) {
	KeyButtonCalls.erase(ID);
}

void InputManager::RemoveMouseScrollCallback(std::vector<int> ID) {
	MouseScrollCalls.erase(ID);
}

void InputManager::OnCursorPosition(GLFWwindow* window, double xpos, double ypos) {
	ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);

	int windowWidth, windowHeight;
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight); // ← change this
	glX = (2.0f * static_cast<float>(xpos) / windowWidth) - 1.0f;
	glY = 1.0f - (2.0f * static_cast<float>(ypos) / windowHeight);
	glX = glX * EngineManager::getInstance().aspectRatio;

	std::vector<std::pair<std::vector<int>, std::function<void(double, double)>>> sortedCalls;

	for (const auto& entry : CursorPositionCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1]; // Sort by priority index
	});

	for (const auto& [id, func] : sortedCalls) {
		func(xpos, ypos);
	}
}

void InputManager::OnMouseButton(GLFWwindow* window, int button, int action, int mods) {
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		mouseLeftHold = true;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		mouseLeftHold = false;
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		mouseRightHold = true;
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
		mouseRightHold = false;
	}

	if (EditorManager::getInstance().WindowHovered) {
		return;
	}

	std::vector<std::pair<std::vector<int>, std::function<void(int, int, int)>>> sortedCalls;

	for (const auto& entry : MouseButtonCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1]; // Sort by priority index
		});

	for (const auto& [id, func] : sortedCalls) {
		func(button, action, mods);
	}
}

void InputManager::OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods) {
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	if (action == GLFW_PRESS) {
		keys[key] = true;
	}
	else if (action == GLFW_RELEASE) {
		keys[key] = false;
	}

	if (EditorManager::getInstance().WindowTyped) {
		return;
	}

	std::vector<std::pair<std::vector<int>, std::function<void(int, int, int, int)>>> sortedCalls;

	for (const auto& entry : KeyButtonCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1]; // Sort by priority index
		});

	for (const auto& [id, func] : sortedCalls) {
		func(key, scancode, action, mods);
	}
}

void InputManager::OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
	ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

	if (EditorManager::getInstance().WindowHovered) {
		return;
	}

	std::vector<std::pair<std::vector<int>, std::function<void(double, double)>>> sortedCalls;

	for (const auto& entry : MouseScrollCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1]; // Sort by priority index
		});

	for (const auto& [id, func] : sortedCalls) {
		func(xoffset, yoffset);
	}
}