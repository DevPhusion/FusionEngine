#include "../../../../Header Files/Core/Editor/Windows/Console.h"
#include "../../../../Header Files/Core/Editor/EditorField.h"
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

		std::ostream& out = (type == MessageType::Error) ? std::cerr : std::cout;
		out << prefix << text << '\x1e' << std::endl;
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

namespace {
	const ImVec4 kInfoColor(0.80f, 0.81f, 0.83f, 1.0f);
	const ImVec4 kWarningColor(0.93f, 0.74f, 0.30f, 1.0f);
	const ImVec4 kErrorColor(0.92f, 0.38f, 0.40f, 1.0f);
	const ImVec4 kMutedColor(0.52f, 0.53f, 0.55f, 1.0f);

	ImVec4 WithAlpha(const ImVec4& c, float a) {
		return ImVec4(c.x, c.y, c.z, a);
	}

	float ChipWidth(const std::string& text) {
		return ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	}

	bool FilterChip(const char* id, const std::string& text, bool* state, const ImVec4& color) {
		const std::string label = text + "###" + id;

		if (*state) {
			ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(color, 0.16f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(color, 0.28f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(color, 0.38f));
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(color, 0.55f));
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Border));
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		bool clicked = ImGui::Button(label.c_str());
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(5);

		if (clicked) *state = !*state;
		return clicked;
	}
}

void Console::ProcessWindow() {
	ImGui::Begin(name.c_str());
	DrawContent();
	ImGui::End();
}

void Console::DrawContent() {
	size_t infoCount, warningCount, errorCount;
	{
		std::lock_guard<std::mutex> lock(mutex);
		infoCount = totalInfoCount;
		warningCount = totalWarningCount;
		errorCount = totalErrorCount;
	}

	if (ImGui::Button("Clear")) {
		Clear();
	}
	ImGui::SameLine();
	EditorField::CheckboxEngine(nullptr, "Autoscroll##autoscroll", &autoScroll);

	const std::string infoText = "Info  " + FormatCount(infoCount);
	const std::string warningText = "Warnings  " + FormatCount(warningCount);
	const std::string errorText = "Errors  " + FormatCount(errorCount);

	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float chipsWidth = ChipWidth(infoText) + ChipWidth(warningText) + ChipWidth(errorText) + spacing * 3.0f;
	ImGui::SameLine();
	float filterWidth = ImGui::GetContentRegionAvail().x - chipsWidth;
	if (filterWidth < 80.0f) filterWidth = 80.0f;

	ImGui::SetNextItemWidth(filterWidth);
	ImGui::InputTextWithHint("##FilterMessages", "Filter messages", filterBuffer, IM_ARRAYSIZE(filterBuffer));

	ImGui::SameLine();
	FilterChip("ShowInfo", infoText, &showInfo, kInfoColor);
	ImGui::SameLine();
	FilterChip("ShowWarnings", warningText, &showWarnings, kWarningColor);
	ImGui::SameLine();
	FilterChip("ShowErrors", errorText, &showErrors, kErrorColor);

	const std::string filterStr(filterBuffer);

	ImGui::BeginChild("ConsoleMessages", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

	{
		std::lock_guard<std::mutex> lock(mutex);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));

		int row = 0;
		for (auto& msg : messages) {
			if (msg.type == MessageType::Info && !showInfo) continue;
			if (msg.type == MessageType::Warning && !showWarnings) continue;
			if (msg.type == MessageType::Error && !showErrors) continue;
			if (!filterStr.empty() && !ContainsCaseInsensitive(msg.text, filterStr)) continue;

			ImVec4 color = kInfoColor;
			ImU32 stripColor = IM_COL32(90, 92, 98, 255);
			ImU32 tintColor = 0;
			if (msg.type == MessageType::Warning) {
				color = kWarningColor;
				stripColor = ImGui::GetColorU32(kWarningColor);
				tintColor = ImGui::GetColorU32(WithAlpha(kWarningColor, 0.07f));
			}
			else if (msg.type == MessageType::Error) {
				color = kErrorColor;
				stripColor = ImGui::GetColorU32(kErrorColor);
				tintColor = ImGui::GetColorU32(WithAlpha(kErrorColor, 0.08f));
			}

			size_t newlinePos = msg.text.find('\n');
			std::string firstLine = (newlinePos == std::string::npos) ? msg.text : msg.text.substr(0, newlinePos);

			const float width = ImGui::GetContentRegionAvail().x;
			const ImVec2 start = ImGui::GetCursorScreenPos();

			dl->ChannelsSplit(2);
			dl->ChannelsSetCurrent(1);

			ImGui::BeginGroup();
			ImGui::Dummy(ImVec2(1.0f, 3.0f));
			ImGui::Indent(14.0f);

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextWrapped("%s", firstLine.c_str());
			ImGui::PopStyleColor();

			if (newlinePos != std::string::npos) {
				std::string rest = msg.text.substr(newlinePos + 1);
				if (!rest.empty()) {
					ImGui::PushStyleColor(ImGuiCol_Text, kMutedColor);
					ImGui::TextWrapped("%s", rest.c_str());
					ImGui::PopStyleColor();
				}
			}

			ImGui::Unindent(14.0f);
			ImGui::Dummy(ImVec2(1.0f, 3.0f));
			ImGui::EndGroup();

			const float endY = ImGui::GetItemRectMax().y;

			dl->ChannelsSetCurrent(0);
			if (tintColor != 0)
				dl->AddRectFilled(start, ImVec2(start.x + width, endY), tintColor);
			else if (row % 2 == 1)
				dl->AddRectFilled(start, ImVec2(start.x + width, endY), IM_COL32(255, 255, 255, 6));
			dl->AddRectFilled(start, ImVec2(start.x + 3.0f, endY), stripColor);
			dl->ChannelsMerge();

			row++;
		}

		ImGui::PopStyleVar();
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
}