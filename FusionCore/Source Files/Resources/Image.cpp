#include "../../Header Files/Resources/Image.h"
#include "../../Header Files/Core/Files/FileManager.h"
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
	// File changed (or first load): drop any stale cached texture so the preview reloads it.
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
	if (tex == 0 || width <= 0 || height <= 0) {
		ImGui::TextDisabled("Preview unavailable (unsupported or corrupt image)");
		return;
	}

	const float availW = ImGui::GetContentRegionAvail().x;
	const float maxH = 360.0f;
	const float scale = std::min(availW / width, maxH / height);
	const ImVec2 size(width * scale, height * scale);

	ImVec2 pos = ImGui::GetCursorScreenPos();
	pos.x += (availW - size.x) * 0.5f;
	ImGui::SetCursorScreenPos(pos);

	const ImVec2 end(pos.x + size.x, pos.y + size.y);
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// Checkerboard behind the image so transparency is visible
	const float tile = 8.0f;
	dl->AddRectFilled(pos, end, IM_COL32(60, 60, 60, 255));
	dl->PushClipRect(pos, end, true);
	int row = 0;
	for (float y = 0; y < size.y; y += tile, row++)
		for (float x = (row % 2) * tile; x < size.x; x += tile * 2)
			dl->AddRectFilled(ImVec2(pos.x + x, pos.y + y), ImVec2(pos.x + x + tile, pos.y + y + tile), IM_COL32(85, 85, 85, 255));
	dl->PopClipRect();

	// UVs are flipped to match how the file tree draws these thumbnails
	ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0, 1), ImVec2(1, 0));
	dl->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border));
}

void Image::DrawProperties() {
	if (!valid) return;

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