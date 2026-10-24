#include "../../Header Files/Resources/Image.h"
#include "../../Header Files/Core/Files/FileManager.h"
#include "../../Header Files/Core/Editor/EditorField.h"
#include <algorithm>
#include <cstdio>
#include <numeric>

std::string Image::GetTypeName() const {
	std::string ext = GetAbsolutePath().extension().string();
	if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::toupper(c); });
	return ext.empty() ? "Image" : ext + " image";
}

void Image::Unload() {
	FileManager::getInstance().InvalidateThumbnail(GetAbsolutePath());
}

void Image::OnReload() {
	FileManager::getInstance().InvalidateThumbnail(GetAbsolutePath());

	width = height = channels = 0;
	is16Bit = false;
	valid = false;

	if (!Exists()) return;

	const std::string path = GetAbsolutePath().string();
	int w = 0, h = 0, c = 0;
	if (!stbi_info(path.c_str(), &w, &h, &c)) return;

	width = w;
	height = h;
	channels = c;
	is16Bit = stbi_is_16_bit(path.c_str()) != 0;
	valid = true;
}

void Image::DrawPreview() {
	unsigned int tex = valid ? FileManager::getInstance().GetOrLoadThumbnail(GetAbsolutePath()) : 0;
	EditorField::Detail::ImagePreview(tex, width, height, previewMaxHeight);
}

void Image::DrawProperties() {
	if (!valid) return;

	using EditorField::PropertyRow;

	PropertyRow("Resolution", std::to_string(width) + " x " + std::to_string(height) + " px");

	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.2f MP", (double)width * height / 1.0e6);
	PropertyRow("Megapixels", buf);

	const int g = std::max(1, std::gcd(width, height));
	std::snprintf(buf, sizeof(buf), "%d:%d (%.3f)", width / g, height / g, (float)width / height);
	PropertyRow("Aspect Ratio", buf);

	static const char* channelNames[] = { "", "Grayscale", "Grayscale + Alpha", "RGB", "RGBA" };
	const int c = std::clamp(channels, 1, 4);
	PropertyRow("Channels", std::to_string(channels) + " (" + channelNames[c] + ")");
	PropertyRow("Bit Depth", is16Bit ? "16 bits / channel" : "8 bits / channel");
	PropertyRow("Alpha", HasAlpha() ? "Yes" : "No");
	PropertyRow("Uncompressed", FormatBytes((uintmax_t)width * height * 4) + " (RGBA8)");
}