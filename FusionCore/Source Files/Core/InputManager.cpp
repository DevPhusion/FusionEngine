#include "../../Header Files/Core/InputManager.h"

float InputManager::glX = 0.0f;
float InputManager::glY = 0.0f;
bool InputManager::mouseLeftHold = false;
bool InputManager::mouseRightHold = false;

std::unordered_map<std::vector<int>, std::function<void(int, int, int)>, VectorHasher> InputManager::MouseButtonCalls = {};
std::unordered_map <std::vector<int>, std::function<void(double, double) >, VectorHasher> InputManager::CursorPositionCalls = {};
std::unordered_map <std::vector<int>, std::function<void(int, int, int, int)>, VectorHasher> InputManager::KeyButtonCalls = {};
std::unordered_map <std::vector<int>, std::function<void(double, double)>, VectorHasher> InputManager::MouseScrollCalls = {};

std::unordered_map<int, bool> InputManager::keys = {};

std::unordered_set<int> InputManager::keysJustPressed = {};
std::unordered_set<int> InputManager::keysJustReleased = {};

std::unordered_map<int, bool> InputManager::mouseButtons = {};
std::unordered_set<int> InputManager::mouseButtonsJustPressed = {};
std::unordered_set<int> InputManager::mouseButtonsJustReleased = {};

std::unordered_map<int, std::unordered_map<int, std::function<void()>>> InputManager::onKeyPressedCallbacks = {};
std::unordered_map<int, std::unordered_map<int, std::function<void()>>> InputManager::onKeyJustPressedCallbacks = {};
std::unordered_map<int, std::unordered_map<int, std::function<void()>>> InputManager::onKeyReleasedCallbacks = {};

std::unordered_map<int, std::unordered_map<int, std::function<void()>>> InputManager::onMouseButtonPressedCallbacks = {};
std::unordered_map<int, std::unordered_map<int, std::function<void()>>> InputManager::onMouseButtonJustPressedCallbacks = {};
std::unordered_map<int, std::unordered_map<int, std::function<void()>>> InputManager::onMouseButtonReleasedCallbacks = {};

void InputManager::Setup(GLFWwindow* window) {
	this->window = window;
	glfwSetMouseButtonCallback(this->window, OnMouseButton);
	glfwSetCursorPosCallback(this->window, OnCursorPosition);
	glfwSetKeyCallback(this->window, OnKeyButton);
	glfwSetScrollCallback(this->window, OnMouseScroll);

	glfwSetCharCallback(this->window, OnCharInput);
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
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
	}

	if (EngineManager::getInstance().isPlayer) {
		ViewportRect vp = EngineManager::getInstance().GetPlayerViewportRect();

		if (vp.width > 0 && vp.height > 0) {
			float localX = (float)xpos - vp.x;
			float localY = (float)ypos - vp.y;

			glX = (2.0f * localX / vp.width) - 1.0f;
			glY = 1.0f - (2.0f * localY / vp.height);
			glX = glX * EngineManager::getInstance().gameAspectRatio;
		}
	}
	else {
		Viewport* gameViewport = EditorManager::getInstance().gameViewport;
		if (gameViewport) {
			ImVec2 panelPos = gameViewport->panelPos;
			ImVec2 panelSize = gameViewport->panelSize;

			if (panelSize.x > 0 && panelSize.y > 0) {
				float localX = (float)xpos - panelPos.x;
				float localY = (float)ypos - panelPos.y;

				glX = (2.0f * localX / panelSize.x) - 1.0f;
				glY = 1.0f - (2.0f * localY / panelSize.y);
				glX = glX * EngineManager::getInstance().gameAspectRatio;
			}
		}
	}

	std::vector<std::pair<std::vector<int>, std::function<void(double, double)>>> sortedCalls;

	for (const auto& entry : CursorPositionCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1];
		});

	for (const auto& [id, func] : sortedCalls) {
		func(xpos, ypos);
	}
}

void InputManager::OnMouseButton(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
	}

	if (action == GLFW_PRESS) {
		mouseButtons[button] = true;
		mouseButtonsJustPressed.insert(button);
	}
	else if (action == GLFW_RELEASE) {
		mouseButtons[button] = false;
		mouseButtonsJustReleased.insert(button);
	}

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

	if (!EngineManager::getInstance().isPlayer && EditorManager::getInstance().WindowHovered) {
		return;
	}

	std::vector<std::pair<std::vector<int>, std::function<void(int, int, int)>>> sortedCalls;

	for (const auto& entry : MouseButtonCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1];
		});

	for (const auto& [id, func] : sortedCalls) {
		func(button, action, mods);
	}
}

void InputManager::OnCharInput(GLFWwindow* window, unsigned int c) {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplGlfw_CharCallback(window, c);
	}
}

void InputManager::OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	}


	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) {
		return;
	}

	if (action == GLFW_PRESS) {
		keys[key] = true;
		keysJustPressed.insert(key);
	}
	else if (action == GLFW_RELEASE) {
		keys[key] = false;
		keysJustReleased.insert(key);
	}

	if (EditorManager::getInstance().WindowTyped) {
		return;
	}

	std::vector<std::pair<std::vector<int>, std::function<void(int, int, int, int)>>> sortedCalls;

	for (const auto& entry : KeyButtonCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1];
		});

	for (const auto& [id, func] : sortedCalls) {
		func(key, scancode, action, mods);
	}
}

void InputManager::OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
	}

	if (EditorManager::getInstance().WindowHovered) {
		return;
	}

	std::vector<std::pair<std::vector<int>, std::function<void(double, double)>>> sortedCalls;

	for (const auto& entry : MouseScrollCalls) {
		sortedCalls.push_back(entry);
	}

	std::sort(sortedCalls.begin(), sortedCalls.end(), [](const auto& a, const auto& b) {
		return a.first[1] > b.first[1];
		});

	for (const auto& [id, func] : sortedCalls) {
		func(xoffset, yoffset);
	}
}

bool InputManager::IsKeyPressed(int key) {
	auto it = keys.find(key);
	return it != keys.end() && it->second;
}

bool InputManager::IsKeyJustPressed(int key) {
	return keysJustPressed.count(key) != 0;
}

bool InputManager::IsKeyReleased(int key) {
	return keysJustReleased.count(key) != 0;
}

bool InputManager::IsMouseButtonPressed(int button) {
	auto it = mouseButtons.find(button);
	return it != mouseButtons.end() && it->second;
}

bool InputManager::IsMouseButtonJustPressed(int button) {
	return mouseButtonsJustPressed.count(button) != 0;
}

bool InputManager::IsMouseButtonReleased(int button) {
	return mouseButtonsJustReleased.count(button) != 0;
}

std::pair<int, int> InputManager::OnKeyPressed(int key, std::function<void()> func) {
	CurrentInputCallbackID += 1;
	onKeyPressedCallbacks[key][CurrentInputCallbackID] = std::move(func);
	return { key, CurrentInputCallbackID };
}

std::pair<int, int> InputManager::OnKeyJustPressed(int key, std::function<void()> func) {
	CurrentInputCallbackID += 1;
	onKeyJustPressedCallbacks[key][CurrentInputCallbackID] = std::move(func);
	return { key, CurrentInputCallbackID };
}

std::pair<int, int> InputManager::OnKeyReleased(int key, std::function<void()> func) {
	CurrentInputCallbackID += 1;
	onKeyReleasedCallbacks[key][CurrentInputCallbackID] = std::move(func);
	return { key, CurrentInputCallbackID };
}

std::pair<int, int> InputManager::OnMouseButtonPressed(int button, std::function<void()> func) {
	CurrentInputCallbackID += 1;
	onMouseButtonPressedCallbacks[button][CurrentInputCallbackID] = std::move(func);
	return { button, CurrentInputCallbackID };
}

std::pair<int, int> InputManager::OnMouseButtonJustPressed(int button, std::function<void()> func) {
	CurrentInputCallbackID += 1;
	onMouseButtonJustPressedCallbacks[button][CurrentInputCallbackID] = std::move(func);
	return { button, CurrentInputCallbackID };
}

std::pair<int, int> InputManager::OnMouseButtonReleased(int button, std::function<void()> func) {
	CurrentInputCallbackID += 1;
	onMouseButtonReleasedCallbacks[button][CurrentInputCallbackID] = std::move(func);
	return { button, CurrentInputCallbackID };
}

void InputManager::RemoveKeyPressedCallback(std::pair<int, int> id) {
	auto it = onKeyPressedCallbacks.find(id.first);
	if (it != onKeyPressedCallbacks.end()) it->second.erase(id.second);
}

void InputManager::RemoveKeyJustPressedCallback(std::pair<int, int> id) {
	auto it = onKeyJustPressedCallbacks.find(id.first);
	if (it != onKeyJustPressedCallbacks.end()) it->second.erase(id.second);
}

void InputManager::RemoveKeyReleasedCallback(std::pair<int, int> id) {
	auto it = onKeyReleasedCallbacks.find(id.first);
	if (it != onKeyReleasedCallbacks.end()) it->second.erase(id.second);
}

void InputManager::RemoveMouseButtonPressedCallback(std::pair<int, int> id) {
	auto it = onMouseButtonPressedCallbacks.find(id.first);
	if (it != onMouseButtonPressedCallbacks.end()) it->second.erase(id.second);
}

void InputManager::RemoveMouseButtonJustPressedCallback(std::pair<int, int> id) {
	auto it = onMouseButtonJustPressedCallbacks.find(id.first);
	if (it != onMouseButtonJustPressedCallbacks.end()) it->second.erase(id.second);
}

void InputManager::RemoveMouseButtonReleasedCallback(std::pair<int, int> id) {
	auto it = onMouseButtonReleasedCallbacks.find(id.first);
	if (it != onMouseButtonReleasedCallbacks.end()) it->second.erase(id.second);
}

void InputManager::DispatchFrameEvents() {
	for (auto& [key, callbacks] : onKeyPressedCallbacks) {
		if (!IsKeyPressed(key)) continue;
		for (auto& [id, func] : callbacks) func();
	}
	for (auto& [button, callbacks] : onMouseButtonPressedCallbacks) {
		if (!IsMouseButtonPressed(button)) continue;
		for (auto& [id, func] : callbacks) func();
	}

	for (int key : keysJustPressed) {
		auto it = onKeyJustPressedCallbacks.find(key);
		if (it == onKeyJustPressedCallbacks.end()) continue;
		for (auto& [id, func] : it->second) func();
	}
	for (int key : keysJustReleased) {
		auto it = onKeyReleasedCallbacks.find(key);
		if (it == onKeyReleasedCallbacks.end()) continue;
		for (auto& [id, func] : it->second) func();
	}
	for (int button : mouseButtonsJustPressed) {
		auto it = onMouseButtonJustPressedCallbacks.find(button);
		if (it == onMouseButtonJustPressedCallbacks.end()) continue;
		for (auto& [id, func] : it->second) func();
	}
	for (int button : mouseButtonsJustReleased) {
		auto it = onMouseButtonReleasedCallbacks.find(button);
		if (it == onMouseButtonReleasedCallbacks.end()) continue;
		for (auto& [id, func] : it->second) func();
	}
}

void InputManager::ClearFrameState() {
	keysJustPressed.clear();
	keysJustReleased.clear();
	mouseButtonsJustPressed.clear();
	mouseButtonsJustReleased.clear();
}