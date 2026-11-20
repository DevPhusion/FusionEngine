#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace RichText {

	struct Piece {
		std::string text;
		bool bold = false, italic = false, underline = false, strike = false;
		bool hasColor = false;          
		glm::vec4 color = glm::vec4(1.0f);
		float size = 0.0f;              
		bool newline = false;
		bool space = false;
	};

	struct Tag {
		char kind = 0;                 
		bool close = false;
		glm::vec4 color = glm::vec4(1.0f);       
		float size = 0.0f;             
	};

	std::vector<Piece> Parse(const std::string& source, bool enableTags = true);

	bool ParseTag(const std::string& rawTag, Tag& out);

	bool ParseColor(const std::string& value, glm::vec4& out);

	std::string WrapSelection(const std::string& source, int selStart, int selEnd,
		const std::string& open, const std::string& close, int* newCaret = nullptr);

	const char* NextUtf8(const char* p, const char* end);
}