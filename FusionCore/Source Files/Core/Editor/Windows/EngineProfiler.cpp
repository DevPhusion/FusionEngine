#include "../../../../Header Files/Core/Editor/Windows/EngineProfiler.h"
#include "../../../../Header Files/Core/Editor/EditorField.h"
#include "../../../../Header Files/Core/Editor/EditorTheme.h"
#include <functional>

namespace {
	bool ToggleButton(const char* label, bool active, bool disabled = false) {
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
}

EngineProfiler::EngineProfiler(std::string name) : EditorWindow(name) {}

void EngineProfiler::ProcessWindow() {
	ImGui::Begin(name.c_str());

	double now = ImGui::GetTime();
	if (startTime < 0.0)
		startTime = now;

	if (now - lastRefresh >= refreshIntervalSeconds) {
		lastRefresh = now;
		snapshot = DebugTimer::GetSnapshot(true);
		UpdateTrackedSeries(snapshot, (float)(now - startTime));
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const bool hasTracked = !trackedSeries.empty();

	if (ToggleButton("Table", !graphView)) graphView = false;
	ImGui::SameLine(0.0f, 2.0f);
	if (ToggleButton("Graph", graphView && hasTracked, !hasTracked)) graphView = true;
	if (!hasTracked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Tick the Track box on one or more tasks to graph them");

	if (hasTracked) {
		const float clearTrackedW = ImGui::CalcTextSize("Clear Tracked").x + style.FramePadding.x * 2.0f;
		const float clearDataW = ImGui::CalcTextSize("Clear Data").x + style.FramePadding.x * 2.0f;
		char countBuf[32];
		std::snprintf(countBuf, sizeof(countBuf), "%d tracked", (int)trackedSeries.size());
		const float countW = ImGui::CalcTextSize(countBuf).x;
		const float total = countW + clearDataW + clearTrackedW + style.ItemSpacing.x * 3.0f;

		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - total);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", countBuf);
		ImGui::SameLine();
		if (ImGui::Button("Clear Data")) {
			for (auto& [key, series] : trackedSeries)
				ClearSeriesData(series);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Tracked")) {
			trackedSeries.clear();
			graphView = false;
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (graphView && !trackedSeries.empty())
		DrawGraphView();
	else
		DrawTableView();

	ImGui::End();
}

void EngineProfiler::DrawTableView() {
	if (snapshot.empty()) {
		ImGui::TextDisabled("No profiling data yet.");
		return;
	}

	const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX;

	if (ImGui::BeginTable("ProfilerTable", 4, flags)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Total (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Calls / Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableSetupColumn("Track", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableHeadersRow();

		std::vector<std::string> pathStack;
		for (auto& root : snapshot)
			DrawNode(root, pathStack);

		ImGui::EndTable();
	}
}

void EngineProfiler::DrawNode(const ProfileNode& node, std::vector<std::string>& pathStack) {
	pathStack.push_back(node.label);
	const std::string key = MakeKey(pathStack);
	const bool isTracked = trackedSeries.find(key) != trackedSeries.end();

	ImGui::TableNextRow(0, ImGui::GetFrameHeight() + 2.0f);
	if (isTracked) {
		const ImVec4 a = EditorTheme::Accent();
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(ImVec4(a.x, a.y, a.z, 0.10f)));
	}

	ImGui::PushID(key.c_str());

	ImGui::TableSetColumnIndex(0);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding |
		ImGuiTreeNodeFlags_DefaultOpen;
	if (node.children.empty())
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	bool open = ImGui::TreeNodeEx("##node", flags, "%s", node.label.c_str());

	ImGui::TableSetColumnIndex(1);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%.3f", node.totalMs);

	ImGui::TableSetColumnIndex(2);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%d / %.3f", node.calls, node.AvgMs());

	ImGui::TableSetColumnIndex(3);
	bool tracked = isTracked;
	if (EditorField::Detail::DrawCheckbox("##track", &tracked)) {
		if (tracked) {
			TrackedSeries series;
			series.path = pathStack;
			series.displayName = key;
			series.color = ColorForKey(key);
			trackedSeries[key] = std::move(series);
		}
		else {
			trackedSeries.erase(key);
			if (trackedSeries.empty())
				graphView = false;
		}
	}

	ImGui::PopID();

	if (open && !node.children.empty()) {
		for (auto& [label, child] : node.children)
			DrawNode(*child, pathStack);
		ImGui::TreePop();
	}

	pathStack.pop_back();
}

void EngineProfiler::DrawGraphView() {
	const ImGuiStyle& style = ImGui::GetStyle();

	const float rowH = ImGui::GetFrameHeight() + 2.0f;
	const float legendH = std::min(rowH * (float)trackedSeries.size() + style.WindowPadding.y * 2.0f, 150.0f);

	if (ImGui::BeginTabBar("ProfilerGraphMetric")) {

		auto drawPlot = [&](const char* plotId, const char* yLabel, ScrollingBuffer TrackedSeries::* member) {
			if (ImPlot::BeginPlot(plotId, ImVec2(-1, -(legendH + style.ItemSpacing.y)), ImPlotFlags_NoTitle)) {
				ImPlot::SetupAxes("Time (s)", yLabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
				for (auto& [key, series] : trackedSeries) {
					ScrollingBuffer& buf = series.*member;
					if (buf.data.empty())
						continue;

					ImPlotSpec spec;
					spec.LineColor = series.color;
					spec.Offset = buf.offset;
					spec.Stride = sizeof(ImVec2);

					ImPlot::PlotLine(series.displayName.c_str(),
						&buf.data[0].x, &buf.data[0].y,
						(int)buf.data.size(), spec);
				}
				ImPlot::EndPlot();
			}
			};

		if (ImGui::BeginTabItem("Total (ms)")) {
			drawPlot("##TotalMsPlot", "Total ms", &TrackedSeries::totalMs);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Average (ms)")) {
			drawPlot("##AvgMsPlot", "Avg ms", &TrackedSeries::avgMs);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Calls")) {
			drawPlot("##CallsPlot", "Calls", &TrackedSeries::calls);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::BeginChild("##ProfilerLegend", ImVec2(0, legendH), ImGuiChildFlags_Borders);

	std::string toRemove;
	for (auto& [key, series] : trackedSeries) {
		ImGui::PushID(key.c_str());

		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float h = ImGui::GetFrameHeight();
		ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 7.0f, p.y + h * 0.5f), 5.0f,
			ImGui::GetColorU32(series.color));
		ImGui::Dummy(ImVec2(16.0f, h));
		ImGui::SameLine();

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(series.displayName.c_str());

		const float resetW = ImGui::CalcTextSize("Reset").x + style.FramePadding.x * 2.0f;
		const float actionsW = resetW + style.ItemSpacing.x + h;
		ImGui::SameLine(ImGui::GetContentRegionMax().x - actionsW);
		if (ImGui::Button("Reset")) ClearSeriesData(series);
		ImGui::SameLine();
		if (EditorTheme::CloseButton("##remove")) toRemove = key;

		ImGui::PopID();
	}

	ImGui::EndChild();

	if (!toRemove.empty())
		trackedSeries.erase(toRemove);

	if (trackedSeries.empty())
		graphView = false;
}

void EngineProfiler::UpdateTrackedSeries(const std::vector<ProfileNode>& roots, float timeX) {
	for (auto& [key, series] : trackedSeries) {
		const ProfileNode* node = FindNodeByPath(roots, series.path);
		if (node) {
			series.totalMs.AddPoint(timeX, (float)node->totalMs);
			series.avgMs.AddPoint(timeX, (float)node->AvgMs());
			series.calls.AddPoint(timeX, (float)node->calls);
		}
		else {
			series.totalMs.AddPoint(timeX, 0.0f);
			series.avgMs.AddPoint(timeX, 0.0f);
			series.calls.AddPoint(timeX, 0.0f);
		}
	}
}

void EngineProfiler::ClearSeriesData(TrackedSeries& series) {
	series.totalMs.Erase();
	series.avgMs.Erase();
	series.calls.Erase();
}

const ProfileNode* EngineProfiler::FindNodeByPath(const std::vector<ProfileNode>& roots, const std::vector<std::string>& path) const {
	if (path.empty())
		return nullptr;

	const ProfileNode* current = nullptr;
	for (auto& root : roots) {
		if (root.label == path[0]) {
			current = &root;
			break;
		}
	}
	if (!current)
		return nullptr;

	for (size_t i = 1; i < path.size(); ++i) {
		auto it = current->children.find(path[i]);
		if (it == current->children.end())
			return nullptr;
		current = it->second.get();
	}
	return current;
}

std::string EngineProfiler::MakeKey(const std::vector<std::string>& path) {
	std::string key;
	for (size_t i = 0; i < path.size(); ++i) {
		if (i) key += "/";
		key += path[i];
	}
	return key;
}

ImVec4 EngineProfiler::ColorForKey(const std::string& key) {
	size_t h = std::hash<std::string>{}(key);
	float hue = (float)(h % 360) / 360.0f;
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(hue, 0.65f, 0.95f, r, g, b);
	return ImVec4(r, g, b, 1.0f);
}