#pragma once
#include "Object.h"
#include "EditorManager.h"
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<functional>
#include<unordered_map>
#include<algorithm>

struct VectorHasher {
	size_t operator()(const std::vector<int>& v) const {
		size_t seed = 0;
		for (int i : v) {
			// Combine hash of each element
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

	void Setup(GLFWwindow* window);
	std::vector<int> SetMouseButtonCallback(std::function<void(int, int, int)> func, int priorityIndex);
	std::vector<int> SetCursorPositionCallback(std::function<void(double, double)> func, int priorityIndex);
	std::vector<int> SetKeyButtonCallback(std::function<void(int, int, int, int)> func, int priorityIndex);
	std::vector<int> SetMouseScrollCallback(std::function<void(double, double)> func, int priorityIndex);

	void RemoveMouseButtonCallback(std::vector<int> ID);
	void RemoveCursorPositionCallback(std::vector<int> ID);
	void RemoveKeyButtonCallback(std::vector<int> ID);
	void RemoveMouseScrollCallback(std::vector<int> ID);

private:
	InputManager() {};

	int CurrentMouseButtonID = -1;
	int CurrentCursorPositionID = -1;
	int CurrentKeyButtonID = -1;
	int CurrentMouseScrollID = -1;

	static void OnCursorPosition(GLFWwindow* window, double xpos, double ypos);
	static void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
	static void OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset);
};

