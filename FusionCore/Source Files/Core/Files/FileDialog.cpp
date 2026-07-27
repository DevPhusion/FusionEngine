#include "../../../Header Files/Core/Files/FileDialog.h"

#include <windows.h>
#include <shobjidl.h>

namespace {
	struct ScopedCom {
		bool ownsInit = false;
		ScopedCom() {
			HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			ownsInit = SUCCEEDED(hr) && hr != S_FALSE;
		}
		~ScopedCom() {
			if (ownsInit) CoUninitialize();
		}
	};
}

std::string FileDialog::WideToUtf8(const std::wstring& wide) {
	if (wide.empty()) return {};
	int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), result.data(), size, nullptr, nullptr);
	return result;
}

std::wstring FileDialog::Utf8ToWide(const std::string& utf8) {
	if (utf8.empty()) return {};
	int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), result.data(), size);
	return result;
}

std::optional<std::string> FileDialog::GetResultPath(IShellItem* item) {
	if (!item) return std::nullopt;

	PWSTR pathW = nullptr;
	HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pathW);
	if (FAILED(hr)) return std::nullopt;

	std::wstring wpath(pathW);
	CoTaskMemFree(pathW);
	return WideToUtf8(wpath);
}

void FileDialog::ApplyOptions(IFileDialog* dialog, const FileDialogOptions& options) {
	if (!options.title.empty())
		dialog->SetTitle(Utf8ToWide(options.title).c_str());

	if (!options.defaultExtension.empty())
		dialog->SetDefaultExtension(Utf8ToWide(options.defaultExtension).c_str());

	if (!options.defaultFileName.empty())
		dialog->SetFileName(Utf8ToWide(options.defaultFileName).c_str());

	if (!options.defaultPath.empty()) {
		IShellItem* folder = nullptr;
		std::wstring pathW = Utf8ToWide(options.defaultPath);
		if (SUCCEEDED(SHCreateItemFromParsingName(pathW.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
			dialog->SetFolder(folder);
			folder->Release();
		}
	}

	if (!options.filters.empty()) {
		std::vector<std::wstring> names, patterns;
		names.reserve(options.filters.size());
		patterns.reserve(options.filters.size());
		for (const auto& f : options.filters) {
			names.push_back(Utf8ToWide(f.name));
			patterns.push_back(Utf8ToWide(f.pattern));
		}

		std::vector<COMDLG_FILTERSPEC> specs;
		specs.reserve(options.filters.size());
		for (size_t i = 0; i < options.filters.size(); i++)
			specs.push_back({ names[i].c_str(), patterns[i].c_str() });

		dialog->SetFileTypes((UINT)specs.size(), specs.data());
		dialog->SetFileTypeIndex(1);
	}
}

std::optional<std::string> FileDialog::ShowSaveDialog(const FileDialogOptions& options) {
	ScopedCom com;

	IFileSaveDialog* dialog = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
		return std::nullopt;

	ApplyOptions(dialog, options);

	std::optional<std::string> result;
	if (SUCCEEDED(dialog->Show(nullptr))) {
		IShellItem* item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item))) {
			result = GetResultPath(item);
			item->Release();
		}
	}

	dialog->Release();
	return result;
}

std::optional<std::string> FileDialog::ShowOpenDialog(const FileDialogOptions& options) {
	ScopedCom com;

	IFileOpenDialog* dialog = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
		return std::nullopt;

	ApplyOptions(dialog, options);

	DWORD flags = 0;
	dialog->GetOptions(&flags);
	dialog->SetOptions(flags | FOS_FILEMUSTEXIST);

	std::optional<std::string> result;
	if (SUCCEEDED(dialog->Show(nullptr))) {
		IShellItem* item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item))) {
			result = GetResultPath(item);
			item->Release();
		}
	}

	dialog->Release();
	return result;
}

std::vector<std::string> FileDialog::ShowOpenDialogMulti(const FileDialogOptions& options) {
	ScopedCom com;

	std::vector<std::string> results;

	IFileOpenDialog* dialog = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
		return results;

	ApplyOptions(dialog, options);

	DWORD flags = 0;
	dialog->GetOptions(&flags);
	dialog->SetOptions(flags | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT);

	if (SUCCEEDED(dialog->Show(nullptr))) {
		IShellItemArray* items = nullptr;
		if (SUCCEEDED(dialog->GetResults(&items))) {
			DWORD count = 0;
			items->GetCount(&count);
			for (DWORD i = 0; i < count; i++) {
				IShellItem* item = nullptr;
				if (SUCCEEDED(items->GetItemAt(i, &item))) {
					if (auto path = GetResultPath(item))
						results.push_back(*path);
					item->Release();
				}
			}
			items->Release();
		}
	}

	dialog->Release();
	return results;
}

std::optional<std::string> FileDialog::ShowFolderDialog(const std::string& title) {
	ScopedCom com;

	IFileOpenDialog* dialog = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
		return std::nullopt;

	if (!title.empty())
		dialog->SetTitle(Utf8ToWide(title).c_str());

	DWORD flags = 0;
	dialog->GetOptions(&flags);
	dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

	std::optional<std::string> result;
	if (SUCCEEDED(dialog->Show(nullptr))) {
		IShellItem* item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item))) {
			result = GetResultPath(item);
			item->Release();
		}
	}

	dialog->Release();
	return result;
}