#include <Core/PDString.h>

#include <windows.h>

std::string toLower(std::string text)
{
	for (char& character : text)
	{
		if (character >= 'A' and character <= 'Z')
		{
			character = (char)(character + ('a' - 'A'));
		}
	}

	return text;
}

std::wstring toWide(std::string const& text)
{
	if (text.empty())
	{
		return std::wstring();
	}

	int const length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);

	if (length <= 0)
	{
		return std::wstring();
	}

	std::wstring wide((std::size_t)length, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wide.data(), length);

	return wide;
}
