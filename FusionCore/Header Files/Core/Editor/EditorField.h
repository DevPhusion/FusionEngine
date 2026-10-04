#pragma once
#include "EditorManager.h"
#include "EditorTheme.h"
#include "../EngineManager.h"
#include "../../../imgui/imgui.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <functional>
#include <cfloat>
#include <cmath>

namespace EditorField {

	namespace Detail {

		inline std::vector<Object*> ToTargets(Object* o) { return { o }; }
		inline std::vector<Object*> ToTargets(std::vector<Object*> v) { return v; }

		inline void Label(const char* label) {
			if (!label) return;
			ImGui::AlignTextToFramePadding();
			const float startX = ImGui::GetCursorPosX();
			const float column = EditorTheme::LabelColumnWidth();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(startX + column);
		}

		inline bool DrawCheckbox(const char* id, bool* v) {
			const float frame = ImGui::GetFrameHeight();
			const float box = ImGui::GetFontSize() + 4.0f;
			const float rounding = ImGui::GetStyle().FrameRounding;

			const char* hashes = std::strstr(id, "##");
			const std::string label = hashes ? std::string(id, hashes - id) : std::string(id);
			const float labelWidth = label.empty() ? 0.0f : ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().ItemInnerSpacing.x;

			ImVec2 p = ImGui::GetCursorScreenPos();
			bool pressed = ImGui::InvisibleButton(id, ImVec2(frame + labelWidth, frame));
			if (pressed) *v = !*v;
			const bool hovered = ImGui::IsItemHovered();
			const bool held = ImGui::IsItemActive();

			ImVec2 mn(p.x + (frame - box) * 0.5f, p.y + (frame - box) * 0.5f);
			ImVec2 mx(mn.x + box, mn.y + box);
			ImDrawList* dl = ImGui::GetWindowDrawList();

			ImU32 fill = ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
			ImU32 border = ImGui::GetColorU32(hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Border);
			dl->AddRectFilled(mn, mx, fill, rounding);
			dl->AddRect(mn, mx, border, rounding, 0, 1.0f);

			if (*v) {
				ImVec2 pts[3] = {
					ImVec2(mn.x + box * 0.25f, mn.y + box * 0.52f),
					ImVec2(mn.x + box * 0.43f, mn.y + box * 0.70f),
					ImVec2(mn.x + box * 0.76f, mn.y + box * 0.30f)
				};
				dl->AddPolyline(pts, 3, ImGui::GetColorU32(ImGuiCol_CheckMark), 0, 2.0f);
			}

			if (!label.empty()) {
				dl->AddText(ImVec2(p.x + frame + ImGui::GetStyle().ItemInnerSpacing.x, p.y + (frame - ImGui::GetFontSize()) * 0.5f),
					ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
			}

			return pressed;
		}

		inline void AxisTag(int axis, float size) {
			static const char* names[] = { "X", "Y", "Z", "W" };
			const char* name = names[axis < 4 ? axis : 3];

			ImVec2 p = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size), EditorTheme::AxisColor(axis),
				ImGui::GetStyle().FrameRounding, ImDrawFlags_RoundCornersLeft);

			ImVec2 ts = ImGui::CalcTextSize(name);
			dl->AddText(ImVec2(p.x + (size - ts.x) * 0.5f, p.y + (size - ts.y) * 0.5f),
				IM_COL32(18, 20, 26, 255), name);
			ImGui::Dummy(ImVec2(size, size));
		}

		inline float WrapDegrees(float d) {
			d = std::fmod(d + 180.0f, 360.0f);
			if (d < 0.0f) d += 360.0f;
			return d - 180.0f;
		}

		inline bool AngleControl(const char* id, float* radians, bool* activated, bool* deactivated) {
			constexpr float kPi = 3.14159265358979f;
			constexpr const char* kPopup = "##anglePopup";

			ImGui::PushID(id);
			ImGuiStorage* st = ImGui::GetStateStorage();
			const ImGuiID editingKey = ImGui::GetID("##editing");
			const ImGuiID wasOpenKey = ImGui::GetID("##wasOpen");

			bool editing = st->GetBool(editingKey);
			bool changed = false;
			bool anyActive = false;

			auto markChanged = [&]() {
				if (!editing) { editing = true; *activated = true; }
				changed = true;
				};

			ImGui::SetNextItemWidth(-FLT_MIN);
			float deg = *radians * 180.0f / kPi;
			if (ImGui::DragFloat("##deg", &deg, 0.5f, 0.0f, 0.0f, "%.1f\xC2\xB0", ImGuiSliderFlags_NoInput)) {
				*radians = WrapDegrees(deg) * kPi / 180.0f;
				markChanged();
			}
			anyActive |= ImGui::IsItemActive();

			if (ImGui::IsItemActivated())
				st->SetBool(wasOpenKey, ImGui::IsPopupOpen(kPopup));  

			if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit() && !st->GetBool(wasOpenKey))
				ImGui::OpenPopup(kPopup);

			const ImVec2 fieldMin = ImGui::GetItemRectMin();
			const ImVec2 fieldMax = ImGui::GetItemRectMax();

			ImGui::SetNextWindowPos(ImVec2(fieldMin.x, fieldMax.y + 4.0f));
			if (ImGui::BeginPopup(kPopup)) {
				const float D = 180.0f;
				const float R = D * 0.5f - 4.0f;

				const ImVec2 p = ImGui::GetCursorScreenPos();
				ImGui::InvisibleButton("##dial", ImVec2(D, D));
				const bool hovered = ImGui::IsItemHovered();
				const bool held = ImGui::IsItemActive();
				anyActive |= held;

				const ImVec2 c(p.x + D * 0.5f, p.y + D * 0.5f);

				if (held) {
					const ImGuiIO& io = ImGui::GetIO();
					const float dx = io.MousePos.x - c.x;
					const float dy = io.MousePos.y - c.y;
					if (dx * dx + dy * dy > 1.0f) {
						float a = std::atan2(-dy, dx);   
						if (io.KeyCtrl) {                
							const float step = 15.0f * kPi / 180.0f;
							a = std::round(a / step) * step;
						}
						if (a != *radians) { *radians = a; markChanged(); }
					}
				}

				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddCircleFilled(c, R, ImGui::GetColorU32(ImGuiCol_FrameBg), 64);
				dl->AddCircle(c, R, ImGui::GetColorU32(hovered || held ? ImGuiCol_SeparatorHovered : ImGuiCol_Border), 64, 1.5f);

				const ImU32 tickCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
				for (int i = 0; i < 24; i++) {
					const float a = (float)i * 15.0f * kPi / 180.0f;
					const ImVec2 dir(std::cos(a), -std::sin(a));
					const float len = (i % 3 == 0) ? 9.0f : 5.0f;
					dl->AddLine(ImVec2(c.x + dir.x * (R - len), c.y + dir.y * (R - len)),
						ImVec2(c.x + dir.x * R, c.y + dir.y * R), tickCol, 1.0f);
				}

				static const int labels[] = { 0, 45, 90, 135, 180, -135, -90, -45 };
				for (int l : labels) {
					const float a = (float)l * kPi / 180.0f;
					char buf[8];
					std::snprintf(buf, sizeof(buf), "%d", l);
					const ImVec2 ts = ImGui::CalcTextSize(buf);
					const float tr = R - 22.0f;
					dl->AddText(ImVec2(c.x + std::cos(a) * tr - ts.x * 0.5f, c.y - std::sin(a) * tr - ts.y * 0.5f),
						ImGui::GetColorU32(ImGuiCol_Text), buf);
				}

				const ImU32 accent = ImGui::GetColorU32(EditorTheme::Accent());
				const float nr = R - 36.0f;
				const ImVec2 tip(c.x + std::cos(*radians) * nr, c.y - std::sin(*radians) * nr);
				dl->AddLine(c, tip, accent, 2.5f);
				dl->AddCircleFilled(tip, 4.0f, accent);
				dl->AddCircleFilled(c, 3.0f, accent);

				ImGui::Spacing();
				ImGui::SetNextItemWidth(D);
				float typed = *radians * 180.0f / kPi;
				if (ImGui::InputFloat("##typed", &typed, 0.0f, 0.0f, "%.1f",
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
					*radians = WrapDegrees(typed) * kPi / 180.0f;
					markChanged();
				}
				anyActive |= ImGui::IsItemActive();

				ImGui::TextDisabled("Ctrl: snap to 15\xC2\xB0");
				ImGui::EndPopup();
			}

			if (editing && !anyActive) {
				editing = false;
				*deactivated = true;
			}
			st->SetBool(editingKey, editing);

			ImGui::PopID();
			return changed;
		}

		template<typename AxisWidget, typename OnActivated, typename OnChanged, typename OnDeactivated>
		bool VecRow(const char* label, const char* id, int n, AxisWidget&& axisWidget,
			OnActivated&& onActivated, OnChanged&& onChanged, OnDeactivated&& onDeactivated) {

			Label(label);
			ImGui::PushID(id);

			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float tag = ImGui::GetFrameHeight();
			const float cellW = (ImGui::GetContentRegionAvail().x - spacing * (n - 1)) / (float)n;

			bool any = false;
			for (int i = 0; i < n; i++) {
				if (i > 0) ImGui::SameLine(0.0f, spacing);
				ImGui::PushID(i);

				AxisTag(i, tag);
				ImGui::SameLine(0.0f, 0.0f);

				ImGui::SetNextItemWidth(cellW - tag);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
				bool changed = axisWidget(i);
				ImGui::PopStyleVar();

				if (ImGui::IsItemActivated()) onActivated();
				if (changed) { onChanged(); any = true; }
				if (ImGui::IsItemDeactivatedAfterEdit()) onDeactivated();

				ImGui::PopID();
			}

			ImGui::PopID();
			return any;
		}

		template<typename Target, typename AxisWidget, typename OnChange>
		bool VecScene(Target&& target, int n, const char* label, const char* id,
			AxisWidget&& axisWidget, OnChange&& onChange) {
			std::vector<Object*> targets = ToTargets(std::forward<Target>(target));
			return VecRow(label, id, n, axisWidget,
				[&] { EditorManager::getInstance().BeginEdit(targets, false); },
				[&] { onChange(); EngineManager::getInstance().SceneChangeEvent(); },
				[&] { EditorManager::getInstance().EndEdit(targets); });
		}

		template<typename AxisWidget>
		bool VecEngine(int n, const char* label, const char* id, AxisWidget&& axisWidget,
			const std::function<void()>& onChange) {
			return VecRow(label, id, n, axisWidget,
				[] {},
				[&] { EngineManager::getInstance().EngineChangeEvent(); if (onChange) onChange(); },
				[] {});
		}
	}

	template<typename WidgetFn, typename OnChange>
	bool Wrap(std::vector<Object*> targets, const char* label, WidgetFn&& widget,
		OnChange&& onChange, bool forceNewEdit = false) {

		if (label) {
			Detail::Label(label);
			ImGui::SetNextItemWidth(-FLT_MIN);
		}

		bool changed = widget();

		if (ImGui::IsItemActivated()) {
			EditorManager::getInstance().BeginEdit(targets, forceNewEdit);
		}
		if (changed) {
			onChange();
			EngineManager::getInstance().SceneChangeEvent();
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
		return Detail::VecScene(std::forward<Target>(target), 2, label, id,
			[&](int i) { return ImGui::InputFloat("##v", &v[i], 0.0f, 0.0f, format); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputFloat3Scene(Target&& target, const char* label, const char* id, float v[3],
		OnChange&& onChange, const char* format = "%.3f") {
		return Detail::VecScene(std::forward<Target>(target), 3, label, id,
			[&](int i) { return ImGui::InputFloat("##v", &v[i], 0.0f, 0.0f, format); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputInt2Scene(Target&& target, const char* label, const char* id, int v[2], OnChange&& onChange) {
		return Detail::VecScene(std::forward<Target>(target), 2, label, id,
			[&](int i) { return ImGui::InputInt("##v", &v[i], 0, 0); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputInt3Scene(Target&& target, const char* label, const char* id, int v[3], OnChange&& onChange) {
		return Detail::VecScene(std::forward<Target>(target), 3, label, id,
			[&](int i) { return ImGui::InputInt("##v", &v[i], 0, 0); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputIntScene(Target&& target, const char* label, const char* id, int* v, OnChange&& onChange) {
		return Wrap(std::forward<Target>(target), label,
			[&] { return ImGui::InputInt(id, v); },
			std::forward<OnChange>(onChange));
	}

	template<typename Target, typename OnChange>
	bool InputAngleScene(Target&& target, const char* label, const char* id, float* radians, OnChange&& onChange) {
		std::vector<Object*> targets = Detail::ToTargets(std::forward<Target>(target));

		Detail::Label(label);
		bool activated = false, deactivated = false;
		bool changed = Detail::AngleControl(id, radians, &activated, &deactivated);

		if (activated) EditorManager::getInstance().BeginEdit(targets, false);
		if (changed) {
			onChange();
			EngineManager::getInstance().SceneChangeEvent();
		}
		if (deactivated) EditorManager::getInstance().EndEdit(targets);
		return changed;
	}

	inline bool InputAngleEngine(const char* label, const char* id, float* radians,
		const std::function<void()>& onChange = nullptr) {
		Detail::Label(label);
		bool activated = false, deactivated = false;
		bool changed = Detail::AngleControl(id, radians, &activated, &deactivated);
		if (changed) {
			EngineManager::getInstance().EngineChangeEvent();
			if (onChange) onChange();
		}
		return changed;
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
		Detail::Label(text);
		bool changed = Detail::DrawCheckbox(id, v);
		if (changed) {
			EditorManager::getInstance().BeginEdit(targets, forceNewEdit);
			onChange();
			EngineManager::getInstance().SceneChangeEvent();
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
			EngineManager::getInstance().SceneChangeEvent();
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
			Detail::Label(label);
			ImGui::SetNextItemWidth(-FLT_MIN);
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
		return Detail::VecEngine(2, label, id,
			[&](int i) { return ImGui::InputFloat("##v", &v[i], 0.0f, 0.0f, format); }, onChange);
	}

	inline bool InputFloat3Engine(const char* label, const char* id, float v[3], const char* format = "%.3f",
		const std::function<void()>& onChange = nullptr) {
		return Detail::VecEngine(3, label, id,
			[&](int i) { return ImGui::InputFloat("##v", &v[i], 0.0f, 0.0f, format); }, onChange);
	}

	inline bool InputIntEngine(const char* label, const char* id, int* v,
		const std::function<void()>& onChange = nullptr) {
		return WrapEngine(label,
			[&] { return ImGui::InputInt(id, v); },
			onChange);
	}

	inline bool InputInt2Engine(const char* label, const char* id, int v[2],
		const std::function<void()>& onChange = nullptr) {
		return Detail::VecEngine(2, label, id,
			[&](int i) { return ImGui::InputInt("##v", &v[i], 0, 0); }, onChange);
	}

	inline bool InputInt3Engine(const char* label, const char* id, int v[3],
		const std::function<void()>& onChange = nullptr) {
		return Detail::VecEngine(3, label, id,
			[&](int i) { return ImGui::InputInt("##v", &v[i], 0, 0); }, onChange);
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
		Detail::Label(text);
		bool changed = Detail::DrawCheckbox(id, v);
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