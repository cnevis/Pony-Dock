#include <UI/PDTrayIcon.h>

#include <shellapi.h>

#include <cwchar>

bool PDTrayIcon::create(HWND window, HICON icon)
{
	m_window = window;
	m_icon = icon;
	m_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

	return add();
}

void PDTrayIcon::destroy()
{
	if (m_window == nullptr)
	{
		return;
	}

	NOTIFYICONDATAW data = {};
	data.cbSize = sizeof(data);
	data.hWnd = m_window;
	data.uID = 1;
	Shell_NotifyIconW(NIM_DELETE, &data);

	m_window = nullptr;
}

PDTrayCommand PDTrayIcon::handleMessage(UINT message, LPARAM lParam, bool running, bool startable)
{
	if (m_window == nullptr)
	{
		return PDTrayCommand::None;
	}

	if (m_taskbarCreated != 0 and message == m_taskbarCreated)
	{
		add();

		return PDTrayCommand::None;
	}

	if (message != TrayMessage)
	{
		return PDTrayCommand::None;
	}

	switch (LOWORD(lParam))
	{
		case WM_LBUTTONUP:
		{
			return PDTrayCommand::Show;
		}

		case WM_RBUTTONUP:
		{
			return showMenu(running, startable);
		}
	}

	return PDTrayCommand::None;
}

bool PDTrayIcon::add()
{
	NOTIFYICONDATAW data = {};
	data.cbSize = sizeof(data);
	data.hWnd = m_window;
	data.uID = 1;
	data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	data.uCallbackMessage = TrayMessage;
	data.hIcon = m_icon;
	wcscpy_s(data.szTip, L"Pony Dock");

	return Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
}

PDTrayCommand PDTrayIcon::showMenu(bool running, bool startable)
{
	HMENU const menu = CreatePopupMenu();

	if (menu == nullptr)
	{
		return PDTrayCommand::None;
	}

	AppendMenuW(menu, MF_STRING, (UINT_PTR)PDTrayCommand::Show, L"Show panel");
	AppendMenuW(menu, running or startable ? MF_STRING : MF_STRING | MF_GRAYED, (UINT_PTR)PDTrayCommand::Toggle, running ? L"Stop" : L"Start");
	AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(menu, MF_STRING, (UINT_PTR)PDTrayCommand::Quit, L"Quit");
	SetMenuDefaultItem(menu, (UINT)PDTrayCommand::Show, FALSE);

	POINT point = {};
	GetCursorPos(&point);
	SetForegroundWindow(m_window);

	int const chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, point.x, point.y, 0, m_window, nullptr);

	PostMessageW(m_window, WM_NULL, 0, 0);
	DestroyMenu(menu);

	return (PDTrayCommand)chosen;
}
