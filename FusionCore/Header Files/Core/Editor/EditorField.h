#pragma once
#include "EditorManager.h"
#include "../EngineManager.h"
#include "../../../imgui/imgui.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <functional>

namespace EditorField {

	template<typename WidgetFn, typename OnChange>
	bool Wrap(std::vector<Object*> targets, const char* label, WidgetFn&& widget,
		OnChange&& onChange, bool forceNewEdit = false) {

		if (label) {
			ImGui::Text("%s", label);
			ImGui::SameLine();
		}

		bool changed = widget();

		if (ImGui::IsItemActivated()) {
			EditorManager::getInstance().BeginEdit(targets, forceNewEdit);
		}
		if (changed) {
			onChange();
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			EditorManager::getInstance().EndEdit(targets);
		}

		return changed;
	}

	template<typename WidgetFn, typename OnChange>
	bool Wrap(Object* target, const char* label, WidgetFn&& widget, OnChange&& onChange) {
		return Wrap(std::vector<Object*>{ target }, label, std::forward<WidgetFn>(widget),
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputFloatScene(Target&& target, const char* label, const char* id, float* v,
		OnChange&& onChange, const char* format = "%.3f") {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::InputFloat(id, v, 0.0f, 0.0f, format); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputFloat2Scene(Target&& target, const char* label, const char* id, float v[2],
		OnChange&& onChange, const char* format = "%.3f") {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::InputFloat2(id, v, format); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputIntScene(Target&& target, const char* label, const char* id, int* v, OnChange&& onChange) {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::InputInt(id, v); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool SliderAngleScene(Target&& target, const char* label, const char* id, float* radians,
		float degMin, float degMax, OnChange&& onChange) {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::SliderAngle(id, radians, degMin, degMax); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool ColorEdit4Scene(Target&& target, const char* label, const char* id, float v[4], OnChange&& onChange) {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::ColorEdit4(id, v); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputTextScene(Target&& target, const char* label, const char* id, char* buf, size_t bufSize,
		OnChange&& onChange, ImGuiInputTextFlags flags = 0) {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::InputText(id, buf, (int)bufSize, flags); },
			std::forward<OnChange>(onChange));
	}

	template<typename OnChange>
	bool CheckboxScene(std::vector<Object*> targets, const char* text, const char* id, bool* v,
		OnChange&& onChange, bool forceNewEdit = false) {
		if (text) {
			ImGui::Text("%s", text);
			ImGui::SameLine();
		}
		bool changed = ImGui::Checkbox(id, v);
		if (changed) {
			EditorManager::getInstance().BeginEdit(targets, forceNewEdit);
			onChange();
			EditorManager::getInstance().EndEdit(targets);
		}
		return changed;
	}

	template<typename OnChange>
	bool CheckboxScene(Object* target, const char* text, const char* id, bool* v, OnChange&& onChange) {
		return CheckboxScene(std::vector<Object*>{ target }, text, id, v, std::forward<OnChange>(onChange));
	}

	template<typename OnChange>
	bool ActionScene(std::vector<Object*> targets, bool triggered, OnChange&& onChange, bool forceNewEdit = false) {
		if (triggered) {
			EditorManager::getInstance().BeginEdit(targets, forceNewEdit);
			onChange();
			EditorManager::getInstance().EndEdit(targets);
		}
		return triggered;
	}

	template<typename OnChange>
	bool ActionScene(Object* target, bool triggered, OnChange&& onChange, bool forceNewEdit = false) {
		return ActionScene(std::vector<Object*>{ target }, triggered, std::forward<OnChange>(onChange), forceNewEdit);
	}

	template<typename WidgetFn>
	inline bool WrapEngine(const char* label, WidgetFn&& widget, const std::function<void()>& onChange = nullptr) {
		if (label) {
			ImGui::Text("%s", label);
			ImGui::SameLine();
		}

		bool changed = widget();

		if (changed) {
			EngineManager::getInstance().EngineChangeEvent();
			if (onChange) {
				onChange();
			}
		}

		return changed;
	}

	inline bool InputFloatEngine(const char* label, const char* id, float* v, const char* format = "%.3f",
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::InputFloat(id, v, 0.0f, 0.0f, format); },
			onChange);
	}

	inline bool InputFloat2Engine(const char* label, const char* id, float v[2], const char* format = "%.3f",
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::InputFloat2(id, v, format); },
			onChange);
	}

	inline bool InputIntEngine(const char* label, const char* id, int* v,
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::InputInt(id, v); },
			onChange);
	}

	inline bool InputInt2Engine(const char* label, const char* id, int v[2],
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::InputInt2(id, v); },
			onChange);
	}

	inline bool SliderAngleEngine(const char* label, const char* id, float* radians, float degMin, float degMax,
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::SliderAngle(id, radians, degMin, degMax); },
			onChange);
	}

	inline bool ColorEdit4Engine(const char* label, const char* id, float v[4],
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::ColorEdit4(id, v); },
			onChange);
	}

	inline bool InputTextEngine(const char* label, const char* id, char* buf, size_t bufSize, ImGuiInputTextFlags flags = 0,
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::InputText(id, buf, (int)bufSize, flags); },
			onChange);
	}

	inline bool CheckboxEngine(const char* text, const char* id, bool* v,
		const std::function<void()>& onChange = nullptr) {
		if (text) {
			ImGui::Text("%s", text);
			ImGui::SameLine();
		}
		bool changed = ImGui::Checkbox(id, v);
		if (changed) {
			EngineManager::getInstance().EngineChangeEvent();
			if (onChange) {
				onChange();
			}
		}
		return changed;
	}

	inline bool ComboEngine(const char* label, const char* id, int* currentIndex, const char* const items[], int itemCount,
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::Combo(id, currentIndex, items, itemCount); },
			onChange);
	}

	inline bool ActionEngine(bool triggered, const std::function<void()>& onChange = nullptr) {
		if (triggered) {
			EngineManager::getInstance().EngineChangeEvent();
			if (onChange) {
				onChange();
			}
		}
		return triggered;
	}

}