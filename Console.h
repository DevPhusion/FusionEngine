#pragma once
#include "EditorWindow.h"
#include <string>
#include <deque>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <type_traits>
#include <glm/glm.hpp>


class Console : public EditorWindow
{
public:
	enum class MessageType { Info, Warning, Error };

	struct Message {
		MessageType type;
		std::string text;
	};

	static constexpr size_t MAX_MESSAGES = 9999;

	class MessageBuilder {
	public:
		MessageBuilder(MessageType type, std::string fmt) : type(type), format(std::move(fmt)) {}

		MessageBuilder(const MessageBuilder&) = delete;
		MessageBuilder& operator=(const MessageBuilder&) = delete;

		MessageBuilder(MessageBuilder&& other) noexcept
			: type(other.type), format(std::move(other.format)), used(other.used) {
			other.used = true;
		}

		~MessageBuilder() {
			if (!used) {
				Console::AddMessage(type, format);
			}
		}

		template<typename... Args>
		void Format(Args&&... args) {
			used = true;
			Console::AddMessage(type, Console::BuildFormatted(format, std::forward<Args>(args)...));
		}

	private:
		MessageType type;
		std::string format;
		bool used = false;
	};

	Console(std::string name);
	Console() = default;

	virtual void ProcessWindow() override;

	static MessageBuilder Print(const std::string& message);
	static MessageBuilder Print(const char* message);
	static MessageBuilder PrintWarning(const std::string& message);
	static MessageBuilder PrintWarning(const char* message);
	static MessageBuilder PrintError(const std::string& message);
	static MessageBuilder PrintError(const char* message);

	template<typename T>
	static void Print(const T& value) { AddMessage(MessageType::Info, ToString(value)); }

	template<typename T>
	static void PrintWarning(const T& value) { AddMessage(MessageType::Warning, ToString(value)); }

	template<typename T>
	static void PrintError(const T& value) { AddMessage(MessageType::Error, ToString(value)); }

	static void Clear();

private:
	static void AddMessage(MessageType type, const std::string& text);
	static std::string FormatCount(size_t count);

	static std::string ToString(bool value) { return value ? "true" : "false"; }
	static std::string ToString(const std::string& value) { return value; }
	static std::string ToString(const char* value) { return std::string(value); }

	template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
	static std::string ToString(T value) {
		std::ostringstream oss;
		oss << value;
		return oss.str();
	}

	template<glm::length_t L, typename T, glm::qualifier Q>
	static std::string ToString(const glm::vec<L, T, Q>& v) {
		std::ostringstream oss;
		oss << "[";
		for (glm::length_t i = 0; i < L; ++i) {
			oss << ToString(v[i]);
			if (i + 1 < L) oss << ", ";
		}
		oss << "]";
		return oss.str();
	}

	template<typename T>
	static std::string ToString(const std::vector<T>& vec) {
		std::ostringstream oss;
		oss << "[";
		for (size_t i = 0; i < vec.size(); ++i) {
			oss << ToString(vec[i]);
			if (i + 1 < vec.size()) oss << ", ";
		}
		oss << "]";
		return oss.str();
	}

	template<typename... Args>
	static std::string BuildFormatted(const std::string& fmt, Args&&... args) {
		std::vector<std::string> parts = { ToString(std::forward<Args>(args))... };
		std::string result;
		result.reserve(fmt.size());

		size_t argIndex = 0;
		for (size_t i = 0; i < fmt.size();) {
			if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i + 1] == '}') {
				result += (argIndex < parts.size()) ? parts[argIndex++] : "{}";
				i += 2;
			}
			else {
				result += fmt[i];
				++i;
			}
		}
		return result;
	}

	static std::deque<Message> messages;
	static std::mutex mutex;

	static size_t totalInfoCount;
	static size_t totalWarningCount;
	static size_t totalErrorCount;

	char filterBuffer[256] = "";
	bool showInfo = true;
	bool showWarnings = true;
	bool showErrors = true;
	bool autoScroll = true;
};