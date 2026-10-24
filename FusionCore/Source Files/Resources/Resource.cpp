#include "../../Header Files/Resources/Resource.h"
#include "../../Header Files/Core/Editor/EditorField.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace fs = std::filesystem;

namespace {
	std::string FormatFileTime(fs::file_time_type t) {
		auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
			t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
		std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

		std::tm tmv{};
#if defined(_MSC_VER)
		localtime_s(&tmv, &tt);
#else
		localtime_r(&tt, &tmv);
#endif
		char buf[64];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
		return buf;
	}
}

Resource::Resource(std::string virtualPath, fs::path absolutePath)
	: virtualPath(std::move(virtualPath)), absolutePath(std::move(absolutePath)) {
}

std::string Resource::GetTypeName() const {
	std::string ext = absolutePath.extension().string();
	if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::toupper(c); });
	return ext.empty() ? "File" : ext + " file";
}

void Resource::Reload() {
	std::error_code ec;
	exists = fs::exists(absolutePath, ec) && !ec;
	fileSize = 0;
	modifiedText.clear();

	if (exists) {
		fileSize = fs::file_size(absolutePath, ec);
		if (ec) fileSize = 0;

		modified = fs::last_write_time(absolutePath, ec);
		if (!ec) modifiedText = FormatFileTime(modified);
	}

	OnReload();
}

bool Resource::CheckForChanges() {
	std::error_code ec;
	const bool nowExists = fs::exists(absolutePath, ec) && !ec;

	bool changed = (nowExists != exists);
	if (nowExists && !changed) {
		auto mt = fs::last_write_time(absolutePath, ec);
		changed = !ec && mt != modified;
	}

	if (changed) Reload();
	return changed;
}

std::string Resource::FormatBytes(uintmax_t bytes) {
	char buf[64];
	if (bytes < 1024)
		std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
	else if (bytes < 1024ull * 1024)
		std::snprintf(buf, sizeof(buf), "%.2f KB (%llu bytes)", bytes / 1024.0, (unsigned long long)bytes);
	else
		std::snprintf(buf, sizeof(buf), "%.2f MB (%llu bytes)", bytes / (1024.0 * 1024.0), (unsigned long long)bytes);
	return buf;
}

void Resource::DrawPreviewWidget(float maxHeight) {
	CheckForChanges();
	previewMaxHeight = maxHeight;
	if (exists) DrawPreview();
	else ImGui::TextDisabled("File not found.");
	previewMaxHeight = -1.0f;
}

void Resource::DrawHeader() {
	EditorField::BoldText(absolutePath.filename().string().c_str());
	ImGui::Spacing();
}

void Resource::DrawCommonProperties() {
	EditorField::PropertyRow("Path", virtualPath, true);
	EditorField::PropertyRow("Location", absolutePath.string(), true);
	EditorField::PropertyRow("Type", GetTypeName());
	EditorField::PropertyRow("File Size", FormatBytes(fileSize));
	if (!modifiedText.empty()) EditorField::PropertyRow("Modified", modifiedText);
}

void Resource::ProcessInspectorUI() {
	CheckForChanges();
	DrawHeader();

	if (!exists) {
		ImGui::TextDisabled("File no longer exists on disk.");
		return;
	}

	DrawPreview();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (EditorField::BeginPropertyTable("##ResourceProps")) {
		DrawCommonProperties();
		DrawProperties();
		EditorField::EndPropertyTable();
	}

	ImGui::Spacing();
	EditorField::CopyButton("Copy Path", virtualPath.c_str());
}