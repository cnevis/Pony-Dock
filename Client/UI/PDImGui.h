#pragma once

#include <d3d11.h>
#include <windows.h>

#include <functional>

class PDImGui
{
public:
	void setFrameCallback(std::function<void()> callback);
	void setMessageCallback(std::function<void(UINT message, LPARAM lParam)> callback);

	bool initialize(HINSTANCE instance, char const* title, int width, int height, HICON icon = nullptr, int minWidth = 0, int minHeight = 0);
	void shutdown();

	bool pumpMessages();
	void show();
	void close();

	void renderFrame();

	void setHideOnClose(bool hide)
	{
		m_hideOnClose = hide;
	}

	bool visible() const
	{
		return IsWindowVisible(m_window) != 0 and IsIconic(m_window) == 0;
	}

	HWND handle() const
	{
		return m_window;
	}

	ID3D11Device* device() const
	{
		return m_device;
	}

private:
	static PDImGui* s_instance;

	static LRESULT CALLBACK staticWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	bool createDevice();
	void destroyDevice();
	bool createRenderTarget();
	void destroyRenderTarget();
	void beginFrame();
	void endFrame();

	HWND m_window = nullptr;
	HINSTANCE m_instance = nullptr;
	bool m_ready = false;
	bool m_rendering = false;
	bool m_hideOnClose = false;
	bool m_occluded = false;
	int m_minClientWidth = 0;
	int m_minClientHeight = 0;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_deviceContext = nullptr;
	IDXGISwapChain* m_swapChain = nullptr;
	ID3D11RenderTargetView* m_renderTargetView = nullptr;

	std::function<void()> m_frameCallback;
	std::function<void(UINT message, LPARAM lParam)> m_messageCallback;
};
