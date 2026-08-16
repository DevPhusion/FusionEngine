#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "Windows/Console.h"
#ifdef _WIN32
#define NOMINMAX    
#include <windows.h>
#endif

class HeadlessMonitor {
public:
	static HeadlessMonitor& getInstance() {
		static HeadlessMonitor instance;
		return instance;
	}

	HeadlessMonitor(const HeadlessMonitor&) = delete;
	HeadlessMonitor& operator=(const HeadlessMonitor&) = delete;

	bool Launch(const std::string& fusionFilePath, std::string& outError);

	void Stop();

	bool IsRunning() const { return running.load(); }

	void ProcessMonitorWindow();

private:
	HeadlessMonitor() = default;
	~HeadlessMonitor();

	Console headlessConsole{ "Headless Console" };

	void ReaderThreadFunc();
	void AppendLine(const std::string& line);

	std::string projectDisplayName;

#ifdef _WIN32
	PROCESS_INFORMATION processInfo{};
	HANDLE stdoutReadHandle = nullptr;
	HANDLE stdoutWriteHandle = nullptr;
#endif

	std::thread readerThread;
	std::atomic<bool> running{ false };
	std::atomic<bool> stopRequested{ false };

	bool autoScroll = true;
};