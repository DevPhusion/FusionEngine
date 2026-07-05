#pragma once
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

struct DebugTimer {
	static inline std::unordered_map<std::string, double> totals;
	static inline std::unordered_map<std::string, int> counts;
	static inline double lastReport = 0.0;

	std::string label;
	std::chrono::high_resolution_clock::time_point start;

	DebugTimer(const std::string& l) : label(l) {
		start = std::chrono::high_resolution_clock::now();
	}
	~DebugTimer() {
		auto end = std::chrono::high_resolution_clock::now();
		double ms = std::chrono::duration<double, std::milli>(end - start).count();
		totals[label] += ms;
		counts[label] += 1;
	}

	static void ReportIfDue(double nowSeconds) {
		if (nowSeconds - lastReport < 1.0) return;
		lastReport = nowSeconds;
		std::cout << "perf (last 1s)" << std::endl;
		for (auto& [label, total] : totals) {
			int c = counts[label];
			std::cout << label << ": total=" << total << "ms  calls=" << c
				<< "  avg=" << (c ? total / c : 0.0) << "ms" << std::endl;
		}
		totals.clear();
		counts.clear();
	}
};

#define TIME_BLOCK(name) DebugTimer _timer_##__LINE__(name)