#include <Core/PDPaths.h>
#include <Core/PDString.h>

#include <windows.h>

#include <filesystem>

static std::filesystem::path executableDirectory()
{
	std::wstring buffer(UNICODE_STRING_MAX_CHARS, L'\0');
	buffer.resize(GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size()));

	return std::filesystem::path(buffer).parent_path();
}

std::string packsDir()
{
	return pathString((executableDirectory() / "Packs").lexically_normal());
}

std::filesystem::path nativePath(std::string const& path)
{
	return std::filesystem::path(toWide(path));
}

std::string pathString(std::filesystem::path const& path)
{
	std::u8string const text = path.generic_u8string();

	return std::string(reinterpret_cast<char const*>(text.data()), text.size());
}
