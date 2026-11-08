#include <UI/PDFileDialog.h>

#include <Core/PDPaths.h>
#include <Core/PDString.h>

#include <commdlg.h>

#include <cassert>
#include <filesystem>
#include <vector>

bool openFileDialog(HWND owner, char const* title, std::string* result)
{
	assert(result != nullptr);

	std::vector<wchar_t> buffer(MAX_PATH * 4, L'\0');
	std::wstring const caption = toWide(title);

	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(OPENFILENAMEW);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"Pony Dock config (*.json)\0*.json\0All files (*.*)\0*.*\0";
	dialog.nFilterIndex = 1;
	dialog.lpstrFile = buffer.data();
	dialog.nMaxFile = (DWORD)buffer.size();
	dialog.lpstrTitle = caption.c_str();
	dialog.lpstrDefExt = L"json";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (GetOpenFileNameW(&dialog) == 0)
	{
		return false;
	}

	*result = pathString(std::filesystem::path(buffer.data()));

	return true;
}

bool saveFileDialog(HWND owner, char const* title, char const* suggestedName, std::string* result)
{
	assert(result != nullptr);

	std::vector<wchar_t> buffer(MAX_PATH * 4, L'\0');
	std::wstring const caption = toWide(title);
	std::wstring const suggested = toWide(suggestedName);

	for (std::size_t i = 0; i < suggested.size() and i + 1 < buffer.size(); i += 1)
	{
		buffer[i] = suggested[i];
	}

	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(OPENFILENAMEW);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"Pony Dock config (*.json)\0*.json\0All files (*.*)\0*.*\0";
	dialog.nFilterIndex = 1;
	dialog.lpstrFile = buffer.data();
	dialog.nMaxFile = (DWORD)buffer.size();
	dialog.lpstrTitle = caption.c_str();
	dialog.lpstrDefExt = L"json";
	dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (GetSaveFileNameW(&dialog) == 0)
	{
		return false;
	}

	*result = pathString(std::filesystem::path(buffer.data()));

	return true;
}
