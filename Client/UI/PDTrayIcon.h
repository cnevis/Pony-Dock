#pragma once

#include <windows.h>

enum class PDTrayCommand
{
	None,
	Show,
	Toggle,
	Quit
};

class PDTrayIcon
{
public:
	bool create(HWND window, HICON icon);
	void destroy();

	PDTrayCommand handleMessage(UINT message, LPARAM lParam, bool running, bool startable);

private:
	static constexpr UINT TrayMessage = WM_APP + 1;

	bool add();
	PDTrayCommand showMenu(bool running, bool startable);

	HWND m_window = nullptr;
	HICON m_icon = nullptr;
	UINT m_taskbarCreated = 0;
};
