#pragma once
#include "../EditorWindow.h"
#include "../../DebugTimer.h"
#include "../../../../imgui/implot.h"
#include <vector>
#include <string>
#include <unordered_map>

struct ScrollingBuffer {
	int maxSize;
	int offset = 0;
	std::vector<ImVec2> data;

	explicit ScrollingBuffer(int size = 600) : maxSize(size) { data.reserve(size); }

	void AddPoint(float x, float y) {
		if ((int)data.size() < maxSize) {
			data.push_back(ImVec2(x, y));
		}
		else {
			data[offset] = ImVec2(x, y);
			offset = (offset + 1) % maxSize;
		}
	}

	void Erase() {
		data.clear();
		offset = 0;
	}
};

struct TrackedSeries {
	std::vector<std::string> path;   
	std::string displayName;      
	ImVec4 color;
	ScrollingBuffer totalMs;
	ScrollingBuffer avgMs;
	ScrollingBuffer calls;
};

class EngineProfiler : public EditorWindow {
public:
	EngineProfiler(std::string name);
	EngineProfiler() = default;
	virtual void ProcessWindow();

private:
	void DrawTableView();
	void DrawGraphView();
	void DrawNode(const ProfileNode& node, std::vector<std::string>& pathStack);
	void UpdateTrackedSeries(const std::vector<ProfileNode>& roots, float timeX);
	const ProfileNode* FindNodeByPath(const std::vector<ProfileNode>& roots, const std::vector<std::string>& path) const;
	void ClearSeriesData(TrackedSeries& series);

	static std::string MakeKey(const std::vector<std::string>& path);
	static ImVec4 ColorForKey(const std::string& key);

	std::vector<ProfileNode> snapshot;
	double lastRefresh = 0.0;
	double refreshIntervalSeconds = 0.25;
	double startTime = -1.0;

	bool graphView = false;
	std::unordered_map<std::string, TrackedSeries> trackedSeries; 
};