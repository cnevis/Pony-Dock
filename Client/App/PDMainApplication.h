#pragma once

#include <Engine/PDDiagnostics.h>
#include <Engine/PDOverlayWindow.h>
#include <Engine/PDScene.h>
#include <Engine/PDSpriteRenderer.h>
#include <Library/PDMonitor.h>
#include <Library/PDSceneEntry.h>
#include <Library/PDSettingsStore.h>
#include <Math/PDRect.h>
#include <UI/PDImGui.h>
#include <UI/PDTrayIcon.h>
#include <UI/Windows/PDMainWindow.h>

#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PDMainApplication
{
public:
	PDMainApplication();

	int run(HINSTANCE instance);

	bool isRunning() const
	{
		return m_sceneRunning;
	}

	void startScene(std::vector<PDSceneEntry> const& entries);
	void stopScene();
	void reshuffle();
	void force(PDForce const& request);
	void requestExit();

	void reloadPack(std::string const& packPath);

	PDSettingsStore& settings()
	{
		return m_settings;
	}

private:
	void runOverlay(HINSTANCE instance);
	void handleMessage(UINT message, LPARAM lParam);
	std::vector<PDRect> walkableRects() const;
	void applyPendingScene();
	void applyPendingForces();
	void applyPendingPacks();

	PDDiagnostics m_diagnostics;
	PDSettingsStore m_settings;
	PDImGui m_host;
	PDTrayIcon m_tray;
	PDMainWindow m_mainWindow;

	PDOverlayWindow m_overlay;
	PDSpriteRenderer m_spriteRenderer;
	PDScene m_scene;
	std::vector<PDMonitor> m_monitors;

	std::thread m_overlayThread;
	std::atomic<bool> m_overlayRunning = false;
	std::atomic<bool> m_sceneRunning = false;

	std::atomic<bool> m_pendingShuffle = false;

	std::mutex m_sceneMutex;
	std::vector<PDSceneEntry> m_pendingEntries;
	bool m_pendingApply = false;
	std::vector<PDForce> m_pendingForces;

	std::mutex m_packsMutex;
	std::vector<std::string> m_pendingPacks;
};
