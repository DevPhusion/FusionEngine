#include "../../../../Header Files/Core/Editor/Windows/EngineProfiler.h"
#include <functional>

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

	if (!trackedSeries.empty()) {
		if (ImGui::Button(graphView ? "Back to Table" : "View Graph"))
			graphView = !graphView;
		ImGui::SameLine();
		if (ImGui::Button("Clear Tracked")) {
			trackedSeries.clear();
			graphView = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Data")) {
			for (auto& [key, series] : trackedSeries)
				ClearSeriesData(series);
		}
		ImGui::Separator();
	}

	if (graphView && !trackedSeries.empty())
		DrawGraphView();
	else
		DrawTableView();

	ImGui::End();
}

void EngineProfiler::DrawTableView() {
	if (ImGui::BeginTable("ProfilerTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Task");
		ImGui::TableSetupColumn("Total (ms)");
		ImGui::TableSetupColumn("Calls / Avg (ms)");
		ImGui::TableSetupColumn("Track");
		ImGui::TableHeadersRow();

		std::vector<std::string> pathStack;
		for (auto& root : snapshot)
			DrawNode(root, pathStack);

		ImGui::EndTable();
	}
}

void EngineProfiler::DrawNode(const ProfileNode& node, std::vector<std::string>& pathStack) {
	pathStack.push_back(node.label);
	std::string key = MakeKey(pathStack);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
	if (node.children.empty())
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	bool open = ImGui::TreeNodeEx(node.label.c_str(), flags);

	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%.3f", node.totalMs);
	ImGui::TableSetColumnIndex(2);
	ImGui::Text("%d / %.3f", node.calls, node.AvgMs());

	ImGui::TableSetColumnIndex(3);
	bool isTracked = trackedSeries.find(key) != trackedSeries.end();
	std::string checkboxId = "##track_" + key;
	if (ImGui::Checkbox(checkboxId.c_str(), &isTracked)) {
		if (isTracked) {
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

	if (open && !node.children.empty()) {
		for (auto& [label, child] : node.children)
			DrawNode(*child, pathStack);
		ImGui::TreePop();
	}

	pathStack.pop_back();
}

void EngineProfiler::DrawGraphView() {
	if (ImGui::BeginTabBar("ProfilerGraphMetric")) {

		auto drawPlot = [&](const char* plotId, const char* yLabel, ScrollingBuffer TrackedSeries::* member) {
			if (ImPlot::BeginPlot(plotId, ImVec2(-1, -1))) {
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

	ImGui::Separator();
	ImGui::Text("Tracked Tasks:");
	for (auto it = trackedSeries.begin(); it != trackedSeries.end(); ) {
		ImGui::ColorButton(("##color_" + it->first).c_str(), it->second.color,
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(12, 12));
		ImGui::SameLine();
		ImGui::TextUnformatted(it->second.displayName.c_str());
		ImGui::SameLine();
		std::string resetId = "Reset##" + it->first;
		if (ImGui::Button(resetId.c_str()))
			ClearSeriesData(it->second);
		ImGui::SameLine();
		std::string removeId = "Remove##" + it->first;
		if (ImGui::Button(removeId.c_str())) {
			it = trackedSeries.erase(it);
			continue;
		}
		++it;
	}

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