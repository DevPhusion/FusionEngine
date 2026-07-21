#include "Console.h"

std::vector<Console::Message> Console::messages;
std::mutex Console::mutex;

Console::Console(std::string name) : EditorWindow(name) {

}

void Console::AddMessage(MessageType type, const std::string& text) {
	std::lock_guard<std::mutex> lock(mutex);
	messages.push_back({ type, text });
}

void Console::Print(const std::string& message) {
	AddMessage(MessageType::Info, message);
}

void Console::PrintWarning(const std::string& message) {
	AddMessage(MessageType::Warning, message);
}

void Console::PrintError(const std::string& message) {
	AddMessage(MessageType::Error, message);
}

void Console::Clear() {
	std::lock_guard<std::mutex> lock(mutex);
	messages.clear();
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

	int infoCount = 0, warningCount = 0, errorCount = 0;
	for (auto& msg : messages) {
		switch (msg.type) {
		case MessageType::Info:    infoCount++;    break;
		case MessageType::Warning: warningCount++; break;
		case MessageType::Error:   errorCount++;   break;
		}
	}

	if (ImGui::Button("Clear")) {
		messages.clear();
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
	ImGui::Text("Info: %d", infoCount);

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.95f, 0.75f, 0.15f, 1.0f));
	ImGui::Checkbox("##ShowWarnings", &showWarnings);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.15f, 1.0f), "Warnings: %d", warningCount);

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.95f, 0.30f, 0.30f, 1.0f));
	ImGui::Checkbox("##ShowErrors", &showErrors);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "Errors: %d", errorCount);

	ImGui::End();
}