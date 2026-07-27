#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>

struct ProfileNode {
	std::string label;
	double totalMs = 0.0;
	int calls = 0;

	std::unordered_map<std::string, std::unique_ptr<ProfileNode>> children;

	double AvgMs() const { return calls > 0 ? totalMs / calls : 0.0; }

	ProfileNode() = default;
	ProfileNode(const ProfileNode& other) { *this = other; }
	ProfileNode& operator=(const ProfileNode& other) {
		label = other.label;
		totalMs = other.totalMs;
		calls = other.calls;
		children.clear();
		for (auto& [key, child] : other.children)
			children[key] = std::make_unique<ProfileNode>(*child);
		return *this;
	}
	ProfileNode(ProfileNode&&) = default;
	ProfileNode& operator=(ProfileNode&&) = default;
};

class DebugTimer {
public:
	explicit DebugTimer(const std::string& label) {
		std::lock_guard<std::mutex> lock(GetMutex());

		ProfileNode*& top = GetStackTop();
		auto& bucket = top ? top->children : GetRoots();

		auto it = bucket.find(label);
		if (it == bucket.end()) {
			auto node = std::make_unique<ProfileNode>();
			node->label = label;
			m_node = node.get();
			bucket[label] = std::move(node);
		}
		else {
			m_node = it->second.get();
		}

		m_parent = top;
		top = m_node; 
		m_start = std::chrono::high_resolution_clock::now();
	}

	~DebugTimer() {
		auto end = std::chrono::high_resolution_clock::now();
		double ms = std::chrono::duration<double, std::milli>(end - m_start).count();

		std::lock_guard<std::mutex> lock(GetMutex());
		m_node->totalMs += ms;
		m_node->calls += 1;
		GetStackTop() = m_parent; 
	}

	
	static std::vector<ProfileNode> GetSnapshot(bool resetAfter = true) {
		std::lock_guard<std::mutex> lock(GetMutex());

		std::vector<ProfileNode> result;
		result.reserve(GetRoots().size());
		for (auto& [label, node] : GetRoots())
			result.push_back(*node);

		if (resetAfter) GetRoots().clear();
		return result;
	}

private:
	ProfileNode* m_node = nullptr;
	ProfileNode* m_parent = nullptr;
	std::chrono::high_resolution_clock::time_point m_start;

	static std::unordered_map<std::string, std::unique_ptr<ProfileNode>>& GetRoots() {
		static std::unordered_map<std::string, std::unique_ptr<ProfileNode>> roots;
		return roots;
	}
	
	static ProfileNode*& GetStackTop() {
		thread_local ProfileNode* top = nullptr;
		return top;
	}
	static std::mutex& GetMutex() {
		static std::mutex m;
		return m;
	}
};

#define TIME_BLOCK(name) DebugTimer _timer_##__LINE__(name)