#include "../../../../Header Files/Core/Editor/Windows/Console.h"
#include "../../../../Header Files/Core/EngineManager.h"

std::deque<Console::Message> Console::messages;
std::mutex Console::mutex;
size_t Console::totalInfoCount = 0;
size_t Console::totalWarningCount = 0;
size_t Console::totalErrorCount = 0;

Console::Console(std::string name) : EditorWindow(name) {

}

void Console::AddMessage(MessageType type, const std::string& text) {
	std::lock_guard<std::mutex> lock(mutex);

	switch (type) {
	case MessageType::Info:    totalInfoCount++;    break;
	case MessageType::Warning: totalWarningCount++; break;
	case MessageType::Error:   totalErrorCount++;   break;
	}

	messages.push_back({ type, text });

	if (messages.size() > MAX_MESSAGES) {
		messages.pop_front();
	}

	if (EngineManager::getInstance().isPlayer) {
		const char* prefix = "[Info] ";
		if (type == MessageType::Warning) prefix = "[Warning] ";
		else if (type == MessageType::Error) prefix = "[Error] ";

		if (type == MessageType::Error)
			std::cerr << prefix << text << std::endl;
		else
			std::cout << prefix << text << std::endl;
	}
}

std::string Console::FormatCount(size_t count) {
	return count > MAX_MESSAGES ? "9999+" : std::to_string(count);
}

Console::MessageBuilder Console::Print(const std::string& message) {
	return MessageBuilder(MessageType::Info, message);
}
Console::MessageBuilder Console::Print(const char* message) {
	return MessageBuilder(MessageType::Info, std::string(message));
}
Console::MessageBuilder Console::PrintWarning(const std::string& message) {
	return MessageBuilder(MessageType::Warning, message);
}
Console::MessageBuilder Console::PrintWarning(const char* message) {
	return MessageBuilder(MessageType::Warning, std::string(message));
}
Console::MessageBuilder Console::PrintError(const std::string& message) {
	return MessageBuilder(MessageType::Error, message);
}
Console::MessageBuilder Console::PrintError(const char* message) {
	return MessageBuilder(MessageType::Error, std::string(message));
}

void Console::Clear() {
	std::lock_guard<std::mutex> lock(mutex);
	messages.clear();
	totalInfoCount = 0;
	totalWarningCount = 0;
	totalErrorCount = 0;
}

static bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
	if (needle.empty()) return true;
	auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
		[](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
	return it != haystack.end();
}

void Console::ProcessWindow() {
	ImGui::Begin(name.c_str());

	std::lock_guard<std::mutex> lock(mutex);

	if (ImGui::Button("Clear")) {
		messages.clear();
		totalInfoCount = 0;
		totalWarningCount = 0;
		totalErrorCount = 0;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Autoscroll", &autoScroll);

	ImGui::Separator();

	std::string filterStr(filterBuffer);
	float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;

	ImGui::BeginChild("ConsoleMessages", ImVec2(0, -bottomBarHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

	for (auto& msg : messages) {
		if (msg.type == MessageType::Info && !showInfo) continue;
		if (msg.type == MessageType::Warning && !showWarnings) continue;
		if (msg.type == MessageType::Error && !showErrors) continue;
		if (!filterStr.empty() && !ContainsCaseInsensitive(msg.text, filterStr)) continue;

		ImVec4 color;
		const char* prefix;
		switch (msg.type) {
		case MessageType::Warning:
			color = ImVec4(0.95f, 0.75f, 0.15f, 1.0f);
			prefix = "[Warning] ";
			break;
		case MessageType::Error:
			color = ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
			prefix = "[Error] ";
			break;
		default:
			color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
			prefix = "";
			break;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextWrapped("%s%s", prefix, msg.text.c_str());
		ImGui::PopStyleColor();
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();

	ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.45f);
	ImGui::InputTextWithHint("##FilterMessages", "Filter Messages", filterBuffer, IM_ARRAYSIZE(filterBuffer));

	ImGui::SameLine();
	ImGui::Checkbox("##ShowInfo", &showInfo);
	ImGui::SameLine();
	ImGui::Text("Info: %s", FormatCount(totalInfoCount).c_str());

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.95f, 0.75f, 0.15f, 1.0f));
	ImGui::Checkbox("##ShowWarnings", &showWarnings);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.15f, 1.0f), "Warnings: %s", FormatCount(totalWarningCount).c_str());

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.95f, 0.30f, 0.30f, 1.0f));
	ImGui::Checkbox("##ShowErrors", &showErrors);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "Errors: %s", FormatCount(totalErrorCount).c_str());

	ImGui::End();
}