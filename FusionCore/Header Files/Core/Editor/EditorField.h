#pragma once
#include "EditorManager.h"
#include "EditorTheme.h"
#include "../EngineManager.h"
#include "../ResourceManager.h"
#include "../../Resources/Resource.h"
#include "../../../imgui/imgui.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <functional>
#include <cfloat>
#include <cmath>

namespace EditorField {
	struct FileSpec {
		enum class Preview { None, Image, Audio };

		FileDialogOptions dialog;                    // title + filters for the browse dialog
		std::string folderTitle = "Choose Folder";   // dialog title when `folder` is true
		std::vector<std::string> extensions;         // lowercase, with dot. Empty = anything
		const char* hint = "None";                   // shown when empty
		bool folder = false;                         // pick folders instead of files
		bool storeVirtual = false;                   // value stored as a virtual path (default: absolute)
		bool displayVirtual = false;                 // show the virtual path while not editing
		bool allowClear = false;                     // show an X to empty the field
		Preview preview = Preview::None;
		float previewHeight = -1.0f;                 // <=0: default

		static FileSpec Any(const char* title = "Choose File") {
			FileSpec s; s.dialog.title = title; return s;
		}
		static FileSpec Extension(const char* description, const char* ext, const char* title) {
			FileSpec s;
			s.dialog = FileDialogOptions::ForExtension(description, ext, title);
			std::string e = std::string(".") + ext;
			for (char& c : e) c = (char)std::tolower((unsigned char)c);
			s.extensions = { e };
			return s;
		}
		static FileSpec Folder(const char* title = "Choose Folder") {
			FileSpec s; s.folder = true; s.folderTitle = title; s.hint = "(no folder selected)"; return s;
		}
		static FileSpec ImageFile() {
			FileSpec s;
			s.dialog.title = "Choose Texture";
			s.dialog.filters = { { "Images", "*.png;*.jpeg;*.jpg" }, { "All Files", "*.*" } };
			s.extensions = { ".png", ".jpg", ".jpeg" };
			s.hint = "None (click to choose...)";
			s.displayVirtual = true;
			s.preview = Preview::Image;
			s.previewHeight = 120.0f;
			return s;
		}
		static FileSpec AudioFile() {
			FileSpec s;
			s.dialog.title = "Choose Audio";
			s.dialog.filters = { { "Audio Files", "*.wav;*.mp3" }, { "All Files", "*.*" } };
			s.extensions = { ".wav", ".mp3" };
			s.hint = "None (click to choose...)";
			s.displayVirtual = true;
			s.preview = Preview::Audio;
			s.previewHeight = 64.0f;
			return s;
		}
	};

	inline bool CloseButton(const char* id); // Forward decalration

	namespace Detail {

		inline std::vector<Object*> ToTargets(Object* o) { return { o }; }
		inline std::vector<Object*> ToTargets(std::vector<Object*> v) { return v; }

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

		inline float LabelColumnWidth() {
			float w = ImGui::GetContentRegionAvail().x * 0.36f;
			return w < 90.0f ? 90.0f : w;
		}

		inline void Label(const char* label) {
			if (!label) return;
			ImGui::AlignTextToFramePadding();
			const float startX = ImGui::GetCursorPosX();
			const float column = LabelColumnWidth();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(startX + column);
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

		inline ImU32 CheckerBackground() { return IM_COL32(60, 60, 60, 255); }
		inline ImU32 CheckerTile() { return IM_COL32(85, 85, 85, 255); }
		inline float CheckerTileSize() { return 8.0f; }
		inline float PreviewMaxHeight() { return 360.0f; }
		inline float WaveformHeight() { return 140.0f; }

		inline std::string FormatTime(double s, bool millis = false) {
			if (s < 0.0) s = 0.0;
			const int total = (int)s;
			char buf[32];
			if (millis) std::snprintf(buf, sizeof(buf), "%d:%02d.%03d", total / 60, total % 60, (int)((s - total) * 1000.0));
			else        std::snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
			return buf;
		}

		inline void ImagePreview(unsigned int tex, int width, int height, float maxHeight = -1.0f) {
			if (tex == 0 || width <= 0 || height <= 0) {
				ImGui::TextDisabled("%s", "Preview unavailable (unsupported or corrupt image)");
				return;
			}
			if (maxHeight <= 0.0f) maxHeight = PreviewMaxHeight();

			const float availW = ImGui::GetContentRegionAvail().x;
			const float scale = std::min(availW / width, maxHeight / height);
			const ImVec2 size(width * scale, height * scale);

			ImVec2 pos = ImGui::GetCursorScreenPos();
			pos.x += (availW - size.x) * 0.5f;
			ImGui::SetCursorScreenPos(pos);

			const ImVec2 end(pos.x + size.x, pos.y + size.y);
			ImDrawList* dl = ImGui::GetWindowDrawList();

			const float tile = CheckerTileSize();
			dl->AddRectFilled(pos, end, CheckerBackground());
			dl->PushClipRect(pos, end, true);
			int row = 0;
			for (float y = 0; y < size.y; y += tile, row++)
				for (float x = (row % 2) * tile; x < size.x; x += tile * 2)
					dl->AddRectFilled(ImVec2(pos.x + x, pos.y + y),
						ImVec2(pos.x + x + tile, pos.y + y + tile), CheckerTile());
			dl->PopClipRect();

			ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0, 1), ImVec2(1, 0));
			dl->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border));
		}

		inline bool PlayButton(const char* id, bool playing) {
			const float size = ImGui::GetFrameHeight();
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
			const bool hovered = ImGui::IsItemHovered();
			const bool held = ImGui::IsItemActive();
			ImDrawList* dl = ImGui::GetWindowDrawList();

			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size),
				ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button),
				ImGui::GetStyle().FrameRounding);

			const ImU32 col = ImGui::GetColorU32(EditorTheme::Accent());
			const ImVec2 c(p.x + size * 0.5f, p.y + size * 0.5f);
			const float r = size * 0.28f;

			if (playing) {
				dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x - r * 0.3f, c.y + r), col);
				dl->AddRectFilled(ImVec2(c.x + r * 0.3f, c.y - r), ImVec2(c.x + r, c.y + r), col);
			}
			else {
				dl->AddTriangleFilled(ImVec2(c.x - r * 0.7f, c.y - r), ImVec2(c.x - r * 0.7f, c.y + r),
					ImVec2(c.x + r, c.y), col);
			}
			return pressed;
		}

		inline bool WaveformPlot(const char* id, const float* xs, const float* mins, const float* maxs,
			int count, float duration, float playhead, float* seekTo, float height = -1.0f) {

			if (height <= 0.0f) height = WaveformHeight();
			bool seeked = false;

			ImGuiStorage* st = ImGui::GetStateStorage();
			const ImGuiID dragKey = ImGui::GetID("##scrubbing");
			bool scrubbing = st->GetBool(dragKey);

			const ImPlotFlags plotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
				ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText;

			if (ImPlot::BeginPlot(id, ImVec2(-1, height), plotFlags)) {
				ImPlot::SetupAxes(nullptr, nullptr,
					ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines,
					ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickLabels);
				ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, (double)duration, ImPlotCond_Always);
				ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0, 1.0, ImPlotCond_Always);

				float shownPlayhead = playhead;
				if (seekTo) {
					const bool hovered = ImPlot::IsPlotHovered();
					if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						scrubbing = true;
					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
						scrubbing = false;

					if (hovered || scrubbing)
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

					if (scrubbing) {
						*seekTo = std::clamp((float)ImPlot::GetPlotMousePos().x, 0.0f, duration);
						shownPlayhead = *seekTo;   
						seeked = true;
					}
				}

				if (count > 0) {
					ImPlotSpec wave;
					wave.LineColor = EditorTheme::Accent();
					wave.FillColor = EditorTheme::Accent();
					wave.FillAlpha = 0.55f;
					ImPlot::PlotShaded("##wave", xs, maxs, mins, count, wave);
				}

				const double x = (double)shownPlayhead;
				ImPlotSpec head;
				head.LineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.9f);
				head.LineWeight = 1.5f;
				ImPlot::PlotInfLines("##playhead", &x, 1, head);

				ImPlot::EndPlot();
			}

			st->SetBool(dragKey, scrubbing);
			return seeked;
		}

		struct AudioPlayerResult {
			bool togglePressed = false;
			bool seeked = false;
			float seekTo = 0.0f;
		};

		inline AudioPlayerResult AudioPlayer(const char* id, bool playing, float position, float duration,
			const float* xs, const float* mins, const float* maxs, int count, float waveHeight = -1.0f) {
			AudioPlayerResult r;
			ImGui::PushID(id);

			if (PlayButton("##play", playing)) r.togglePressed = true;
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("%s / %s", FormatTime(position).c_str(), FormatTime(duration).c_str());

			if (count > 0)
				r.seeked = WaveformPlot("##wave", xs, mins, maxs, count, duration, position, &r.seekTo, waveHeight);

			ImGui::PopID();
			return r;
		}

		inline void DrawFolderGlyph(ImDrawList* dl, ImVec2 center, float size, ImU32 col) {
			const float w = size * 0.62f, h = size * 0.46f;
			const float oy = size * 0.04f;
			const ImVec2 mn(center.x - w * 0.5f, center.y - h * 0.5f + oy);
			const ImVec2 mx(center.x + w * 0.5f, center.y + h * 0.5f + oy);
			dl->AddRectFilled(ImVec2(mn.x, mn.y - h * 0.18f), ImVec2(mn.x + w * 0.4f, mn.y + h * 0.3f), col, 1.5f);
			dl->AddRectFilled(mn, mx, col, 1.5f);
		}

		inline bool HasAllowedExtension(const std::filesystem::path& p, const FileSpec& spec) {
			if (spec.extensions.empty()) return true;
			std::string ext = p.extension().string();
			for (char& c : ext) c = (char)std::tolower((unsigned char)c);
			return std::find(spec.extensions.begin(), spec.extensions.end(), ext) != spec.extensions.end();
		}

		inline bool DropAccepts(const std::string& virtualPath, const FileSpec& spec, std::string& absOut) {
			FileManager& fm = FileManager::getInstance();
			if (fm.IsDirectory(virtualPath) != spec.folder) return false;
			const std::filesystem::path abs = fm.VirtualToAbsolute(virtualPath);
			if (!spec.folder && !HasAllowedExtension(abs, spec)) return false;
			absOut = abs.string();
			return true;
		}

		inline bool ResolveTypedPath(const char* text, const FileSpec& spec, std::string& out) {
			namespace fs = std::filesystem;
			FileManager& fm = FileManager::getInstance();

			std::string t(text);
			const size_t b = t.find_first_not_of(" \t");
			const size_t e = t.find_last_not_of(" \t");
			t = (b == std::string::npos) ? std::string() : t.substr(b, e - b + 1);

			if (t.empty()) { out.clear(); return true; }

			std::error_code ec;
			const bool virt = spec.displayVirtual || spec.storeVirtual;
			fs::path resolved;
			if (fs::exists(fs::path(t), ec)) resolved = t;
			else if (virt && fs::exists(fm.VirtualToAbsolute(t), ec)) resolved = fm.VirtualToAbsolute(t);
			else { out = t; return true; }   

			if (fs::is_directory(resolved, ec) != spec.folder) return false;
			if (!spec.folder && !HasAllowedExtension(resolved, spec)) return false;

			out = spec.storeVirtual ? fm.AbsoluteToVirtual(resolved.string()) : resolved.string();
			return true;
		}

		inline bool FileControl(const char* label, const char* id, const std::string& current,
			const FileSpec& spec, std::string& result) {

			FileManager& fm = FileManager::getInstance();
			const ImGuiStyle& style = ImGui::GetStyle();
			const float frame = ImGui::GetFrameHeight();
			const float gap = style.ItemInnerSpacing.x;

			if (label) {
				Label(label);
				ImGui::SetNextItemWidth(-FLT_MIN);
			}
			const float total = ImGui::CalcItemWidth();
			const float reserved = (frame + gap) * (spec.allowClear ? 2.0f : 1.0f);

			ImGui::PushID(id);
			bool changed = false;

			static std::unordered_map<ImGuiID, std::array<char, 512>> buffers;
			const ImGuiID textId = ImGui::GetID("##path");
			const ImGuiID editingKey = ImGui::GetID("##editing");
			ImGuiStorage* st = ImGui::GetStateStorage();
			std::array<char, 512>& buf = buffers[textId];

			if (!st->GetBool(editingKey)) {
				std::string shown = current;
				if (spec.displayVirtual && !spec.storeVirtual && !current.empty())
					shown = fm.AbsoluteToVirtual(current);
				std::snprintf(buf.data(), buf.size(), "%s", shown.c_str());
			}

			auto acceptDrop = [&]() {
				if (!ImGui::BeginDragDropTarget()) return;
				const ImGuiPayload* p = ImGui::AcceptDragDropPayload(
					FileManager::kResourceDragDropPayloadType, ImGuiDragDropFlags_AcceptPeekOnly);
				std::string abs;
				if (p && p->Data) {
					const std::string vp(static_cast<const char*>(p->Data));
					if (DropAccepts(vp, spec, abs)) {
						ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
							ImGui::GetColorU32(EditorTheme::Accent()), style.FrameRounding, 0, 2.0f);
						if (p->IsDelivery()) {
							result = spec.storeVirtual ? vp : abs;
							changed = true;
						}
					}
				}
				ImGui::EndDragDropTarget();
				};

			ImGui::SetNextItemWidth(total - reserved);
			ImGui::InputTextWithHint("##path", spec.hint, buf.data(), buf.size());
			const bool typedCommit = ImGui::IsItemDeactivatedAfterEdit();
			if (ImGui::IsItemActivated())   st->SetBool(editingKey, true);
			if (ImGui::IsItemDeactivated()) st->SetBool(editingKey, false);
			acceptDrop();

			if (typedCommit) {
				std::string resolved;
				if (ResolveTypedPath(buf.data(), spec, resolved) && resolved != current) {
					result = resolved;
					changed = true;
				}
			}

			if (spec.allowClear) {
				ImGui::SameLine(0.0f, gap);
				if (!current.empty()) {
					if (CloseButton("##clear")) { result.clear(); changed = true; }
				}
				else {
					ImGui::Dummy(ImVec2(frame, frame));
				}
			}

			ImGui::SameLine(0.0f, gap);
			const bool browse = ImGui::Button("##browse", ImVec2(frame, frame));
			{
				const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
				DrawFolderGlyph(ImGui::GetWindowDrawList(), ImVec2((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f),
					frame, ImGui::GetColorU32(ImGuiCol_Text));
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(spec.folder ? "Choose folder..." : "Choose file...");
			acceptDrop();

			if (browse) {
				if (spec.folder) {
					if (auto f = FileDialog::ShowFolderDialog(spec.folderTitle.c_str())) {
						result = spec.storeVirtual ? fm.AbsoluteToVirtual(*f) : *f;
						changed = true;
					}
				}
				else {
					FileDialogOptions opts = spec.dialog;
					if (auto p = FileDialog::ShowOpenDialog(opts)) {
						result = spec.storeVirtual ? fm.AbsoluteToVirtual(*p) : *p;
						changed = true;
					}
				}
			}

			if (spec.preview != FileSpec::Preview::None && !current.empty()) {
				const std::string vp = spec.storeVirtual ? current : fm.AbsoluteToVirtual(current);
				if (Resource* res = ResourceManager::getInstance().GetResource(vp))
					res->DrawPreviewWidget(spec.previewHeight);
				else
					ImGui::TextDisabled("Preview unavailable");
				ImGui::Spacing();
			}

			ImGui::PopID();
			return changed;
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

	
	template<typename Target, typename OnChange>
	bool FileScene(Target&& target, const char* label, const char* id, std::string& path,
		OnChange&& onChange, const FileSpec& spec = FileSpec::Any()) {
		std::vector<Object*> targets = Detail::ToTargets(std::forward<Target>(target));
		std::string result;
		if (!Detail::FileControl(label, id, path, spec, result)) return false;

		EditorManager::getInstance().BeginEdit(targets, false);
		path = result;
		onChange();
		EngineManager::getInstance().SceneChangeEvent();
		EditorManager::getInstance().EndEdit(targets);
		return true;
	}

	template<typename Target, typename OnChange>
	bool FolderScene(Target&& target, const char* label, const char* id, std::string& path,
		OnChange&& onChange, const char* dialogTitle = "Choose Folder") {
		return FileScene(std::forward<Target>(target), label, id, path, std::forward<OnChange>(onChange),
			FileSpec::Folder(dialogTitle));
	}

	template<typename Target, typename OnChange>
	bool ImageFileScene(Target&& target, const char* label, const char* id, std::string& path,
		OnChange&& onChange, const FileSpec& spec = FileSpec::ImageFile()) {
		return FileScene(std::forward<Target>(target), label, id, path, std::forward<OnChange>(onChange), spec);
	}

	template<typename Target, typename OnChange>
	bool AudioFileScene(Target&& target, const char* label, const char* id, std::string& path,
		OnChange&& onChange, const FileSpec& spec = FileSpec::AudioFile()) {
		return FileScene(std::forward<Target>(target), label, id, path, std::forward<OnChange>(onChange), spec);
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

	inline bool FileEngine(const char* label, const char* id, std::string& path,
		const FileSpec& spec = FileSpec::Any(), const std::function<void()>& onChange = nullptr) {
		std::string result;
		if (!Detail::FileControl(label, id, path, spec, result)) return false;

		path = result;
		EngineManager::getInstance().EngineChangeEvent();
		if (onChange) onChange();
		return true;
	}

	inline bool FolderEngine(const char* label, const char* id, std::string& path,
		const char* dialogTitle = "Choose Folder", const std::function<void()>& onChange = nullptr) {
		return FileEngine(label, id, path, FileSpec::Folder(dialogTitle), onChange);
	}

	inline bool ImageFileEngine(const char* label, const char* id, std::string& path,
		const FileSpec& spec = FileSpec::ImageFile(), const std::function<void()>& onChange = nullptr) {
		return FileEngine(label, id, path, spec, onChange);
	}

	inline bool AudioFileEngine(const char* label, const char* id, std::string& path,
		const FileSpec& spec = FileSpec::AudioFile(), const std::function<void()>& onChange = nullptr) {
		return FileEngine(label, id, path, spec, onChange);
	}

	inline bool AddButton(const char* label, bool compact = false) {
		ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::AccentDim());
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorTheme::Accent());
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorTheme::Accent());
		bool pressed = compact ? ImGui::SmallButton(label) : ImGui::Button(label);
		ImGui::PopStyleColor(3);
		return pressed;
	}

	inline bool DangerButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.18f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58f, 0.22f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.15f, 0.17f, 1.0f));
		bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return pressed;
	}

	inline bool CloseButton(const char* id) {
		const float size = ImGui::GetFrameHeight();
		ImVec2 p = ImGui::GetCursorScreenPos();
		bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
		const bool hovered = ImGui::IsItemHovered();
		const bool held = ImGui::IsItemActive();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		if (hovered || held) {
			dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size),
				held ? IM_COL32(179, 46, 61, 255) : IM_COL32(230, 77, 89, 217), ImGui::GetStyle().FrameRounding);
		}

		ImVec2 c(p.x + size * 0.5f, p.y + size * 0.5f);
		const float r = 4.0f;
		ImU32 col = hovered || held ? IM_COL32(255, 255, 255, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
		dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 1.6f);
		dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, 1.6f);

		return pressed;
	}

	inline bool ToggleButton(const char* label, bool active, bool disabled = false) {
		ImGui::BeginDisabled(disabled);
		if (active) {
			const ImVec4 a = EditorTheme::Accent();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(a.x, a.y, a.z, 0.25f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.x, a.y, a.z, 0.35f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(a.x, a.y, a.z, 0.45f));
			ImGui::PushStyleColor(ImGuiCol_Text, a);
		}
		bool pressed = ImGui::Button(label);
		if (active) ImGui::PopStyleColor(4);
		ImGui::EndDisabled();
		return pressed;
	}

	inline void BoldText(const char* text, const ImVec4* color = nullptr) {
		bool b = EditorTheme::PushBold();
		if (color) ImGui::PushStyleColor(ImGuiCol_Text, *color);
		ImGui::TextUnformatted(text);
		if (color) ImGui::PopStyleColor();
		EditorTheme::PopBold(b);
	}

	inline bool Checkbox(const char* id, bool* v) {
		return Detail::DrawCheckbox(id, v);
	}

	inline bool BeginPropertyTable(const char* id) {
		if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
			return false;
		ImGui::TableSetupColumn("##k", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
		return true;
	}

	inline void EndPropertyTable() {
		ImGui::EndTable();
	}

	inline void PropertyRow(const char* label, const char* value, bool wrap = false) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%s", label);
		ImGui::TableSetColumnIndex(1);
		if (wrap) ImGui::TextWrapped("%s", value);
		else      ImGui::TextUnformatted(value);
	}

	inline void PropertyRow(const char* label, const std::string& value, bool wrap = false) {
		PropertyRow(label, value.c_str(), wrap);
	}

	inline bool CopyButton(const char* label, const char* text) {
		const bool pressed = ImGui::Button(label);
		if (pressed) ImGui::SetClipboardText(text);
		return pressed;
	}
}