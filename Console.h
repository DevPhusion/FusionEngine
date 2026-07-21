#pragma once
#include "EditorWindow.h"
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cctype>

class Console : public EditorWindow
{
public:
	enum class MessageType { Info, Warning, Error };

	struct Message {
		MessageType type;
		std::string text;
	};

	Console(std::string name);
	Console() = default;

	virtual void ProcessWindow() override;

	static void Print(const std::string& message);
	static void PrintWarning(const std::string& message);
	static void PrintError(const std::string& message);
	static void Clear();

private:
	static void AddMessage(MessageType type, const std::string& text);

	static std::vector<Message> messages;
	static std::mutex mutex;

	char filterBuffer[256] = "";
	bool showInfo = true;
	bool showWarnings = true;
	bool showErrors = true;
	bool autoScroll = true;
};