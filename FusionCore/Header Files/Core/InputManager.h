#pragma once
#include "../Objects/Object.h"
#include "Editor/EditorManager.h"
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<functional>
#include<unordered_map>
#include<unordered_set>
#include<algorithm>
#include<utility>

struct VectorHasher {
	size_t operator()(const std::vector<int>& v) const {
		size_t seed = 0;
		for (int i : v) {
			seed ^= std::hash<int>{}(i)+0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};

class InputManager
{
public:
	InputManager(const InputManager&) = delete;
	void operator=(const InputManager&) = delete;

	static InputManager& getInstance() {
		static InputManager instance;
		return instance;
	}

	GLFWwindow* window;
	static float glX;
	static float glY;
	static bool mouseLeftHold;
	static bool mouseRightHold;

	static std::unordered_map <std::vector<int>, std::function<void(int, int, int)>, VectorHasher> MouseButtonCalls;
	static std::unordered_map <std::vector<int>, std::function<void(double, double)>, VectorHasher> CursorPositionCalls;
	static std::unordered_map <std::vector<int>, std::function<void(int, int, int, int)>, VectorHasher> KeyButtonCalls;
	static std::unordered_map <std::vector<int>, std::function<void(double, double)>, VectorHasher> MouseScrollCalls;
	static std::unordered_map<int, bool> keys;

	static std::unordered_set<int> keysJustPressed;
	static std::unordered_set<int> keysJustReleased;

	static std::unordered_map<int, bool> mouseButtons;
	static std::unordered_set<int> mouseButtonsJustPressed;
	static std::unordered_set<int> mouseButtonsJustReleased;

	static std::unordered_map<int, std::unordered_map<int, std::function<void()>>> onKeyPressedCallbacks;
	static std::unordered_map<int, std::unordered_map<int, std::function<void()>>> onKeyJustPressedCallbacks;
	static std::unordered_map<int, std::unordered_map<int, std::function<void()>>> onKeyReleasedCallbacks;

	static std::unordered_map<int, std::unordered_map<int, std::function<void()>>> onMouseButtonPressedCallbacks;
	static std::unordered_map<int, std::unordered_map<int, std::function<void()>>> onMouseButtonJustPressedCallbacks;
	static std::unordered_map<int, std::unordered_map<int, std::function<void()>>> onMouseButtonReleasedCallbacks;

	void Setup(GLFWwindow* window);
	std::vector<int> SetMouseButtonCallback(std::function<void(int, int, int)> func, int priorityIndex);
	std::vector<int> SetCursorPositionCallback(std::function<void(double, double)> func, int priorityIndex);
	std::vector<int> SetKeyButtonCallback(std::function<void(int, int, int, int)> func, int priorityIndex);
	std::vector<int> SetMouseScrollCallback(std::function<void(double, double)> func, int priorityIndex);

	void RemoveMouseButtonCallback(std::vector<int> ID);
	void RemoveCursorPositionCallback(std::vector<int> ID);
	void RemoveKeyButtonCallback(std::vector<int> ID);
	void RemoveMouseScrollCallback(std::vector<int> ID);

	bool IsKeyPressed(int key);
	bool IsKeyJustPressed(int key);
	bool IsKeyReleased(int key);

	bool IsMouseButtonPressed(int button);
	bool IsMouseButtonJustPressed(int button);
	bool IsMouseButtonReleased(int button);

	std::pair<int, int> OnKeyPressed(int key, std::function<void()> func);
	std::pair<int, int> OnKeyJustPressed(int key, std::function<void()> func);
	std::pair<int, int> OnKeyReleased(int key, std::function<void()> func);

	std::pair<int, int> OnMouseButtonPressed(int button, std::function<void()> func);
	std::pair<int, int> OnMouseButtonJustPressed(int button, std::function<void()> func);
	std::pair<int, int> OnMouseButtonReleased(int button, std::function<void()> func);

	void RemoveKeyPressedCallback(std::pair<int, int> id);
	void RemoveKeyJustPressedCallback(std::pair<int, int> id);
	void RemoveKeyReleasedCallback(std::pair<int, int> id);
	void RemoveMouseButtonPressedCallback(std::pair<int, int> id);
	void RemoveMouseButtonJustPressedCallback(std::pair<int, int> id);
	void RemoveMouseButtonReleasedCallback(std::pair<int, int> id);

	void DispatchFrameEvents();
	void ClearFrameState();

private:
	InputManager() {};

	int CurrentMouseButtonID = -1;
	int CurrentCursorPositionID = -1;
	int CurrentKeyButtonID = -1;
	int CurrentMouseScrollID = -1;
	int CurrentInputCallbackID = -1;

	static void OnCursorPosition(GLFWwindow* window, double xpos, double ypos);
	static void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
	static void OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset);
	static void OnCharInput(GLFWwindow* window, unsigned int c);
};