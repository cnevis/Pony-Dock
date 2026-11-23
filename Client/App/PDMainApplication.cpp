#include <App/PDMainApplication.h>

#include <Assets/resource.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

PDMainApplication::PDMainApplication()
	: m_mainWindow(*this, m_imgui, m_diagnostics)
{
}

int PDMainApplication::run(HINSTANCE instance)
{
	m_imgui.setFrameCallback([this]()
	{
		m_mainWindow.draw();
	});

	m_imgui.setMessageCallback([this](UINT message, LPARAM lParam)
	{
		handleMessage(message, lParam);
	});

	HICON const icon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPICON));

	if (not m_imgui.initialize(instance, "Pony Dock", 780, 600, icon, 610, 320))
	{
		return 1;
	}

	HICON const trayIcon = static_cast<HICON>(LoadImage(instance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));

	if (m_tray.create(m_imgui.handle(), trayIcon))
	{
		m_imgui.setHideOnClose(true);
	}
	else
	{
		m_diagnostics.write("Tray icon failed to initialize");
	}

	m_settings.load();
	m_mainWindow.restoreScene();
	m_diagnostics.write("Settings at " + m_settings.path());

	m_overlayRunning = true;
	m_overlayThread = std::thread([this, instance]()
	{
		runOverlay(instance);
	});

	while (m_imgui.pumpMessages())
	{
		if (not m_imgui.visible())
		{
			WaitMessage();

			continue;
		}

		m_imgui.renderFrame();
	}

	m_overlayRunning = false;

	if (m_overlayThread.joinable())
	{
		m_overlayThread.join();
	}

	m_settings.save();
	m_imgui.shutdown();

	return 0;
}

void PDMainApplication::runOverlay(HINSTANCE instance)
{
	if (not m_overlay.initialize(instance))
	{
		m_diagnostics.write("Overlay window failed to initialize");

		return;
	}

	if (not m_spriteRenderer.initialize(m_overlay.device()))
	{
		m_diagnostics.write("Sprite renderer failed to initialize");
		m_overlay.shutdown();

		return;
	}

	m_monitors = enumerateMonitors();
	m_scene.initialize(m_overlay.device(), &m_diagnostics, &m_settings);

	m_diagnostics.write("Found " + std::to_string(m_monitors.size()) + " displays");

	m_diagnostics.write("Overlay ready at " + std::to_string(m_overlay.width()) + "x" + std::to_string(m_overlay.height()));

	std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
	bool idle = false;

	while (m_overlayRunning)
	{
		m_overlay.pumpMessages();

		if (m_overlay.refreshBounds())
		{
			m_monitors = enumerateMonitors();
			m_diagnostics.write("Displays changed, overlay now " + std::to_string(m_overlay.width()) + "x" + std::to_string(m_overlay.height()) + " over " + std::to_string(m_monitors.size()) + " displays");
		}

		if (m_pendingShuffle.exchange(false))
		{
			m_scene.reshuffle();
		}

		applyPendingPacks();
		applyPendingScene();
		applyPendingForces();

		if (idle and m_scene.empty())
		{
			m_overlay.pollMouse();
			MsgWaitForMultipleObjects(0, nullptr, FALSE, 50, QS_ALLINPUT);
			lastTick = std::chrono::steady_clock::now();

			continue;
		}

		std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
		float const elapsedSeconds = std::chrono::duration<float>(now - lastTick).count();
		float const deltaSeconds = std::min(elapsedSeconds, 0.1f);
		lastTick = now;

		PDOverlayMouse const mouse = m_overlay.pollMouse();
		m_scene.updateMouse(mouse.x, mouse.y, mouse.pressed, mouse.released);
		m_overlay.setInteractive(m_scene.wantsMouse());

		m_scene.tick(deltaSeconds, m_overlay.width(), m_overlay.height(), walkableRects());

		m_overlay.beginFrame();
		m_spriteRenderer.begin(m_overlay.context(), m_overlay.width(), m_overlay.height());
		m_scene.draw(m_spriteRenderer);
		m_spriteRenderer.end();
		m_overlay.endFrame();

		idle = m_scene.empty();
	}

	m_spriteRenderer.shutdown();
	m_overlay.shutdown();
}

std::vector<PDRect> PDMainApplication::walkableRects() const
{
	std::vector<PDRect> rects;

	for (PDMonitor const& monitor : m_monitors)
	{
		if (not m_settings.appFlag("monitor." + monitor.device, true))
		{
			continue;
		}

		rects.push_back(overlayRect(monitor));
	}

	if (not rects.empty() or m_monitors.empty())
	{
		return rects;
	}

	rects.push_back(overlayRect(m_monitors.front()));

	return rects;
}

PDRect PDMainApplication::overlayRect(PDMonitor const& monitor) const
{
	PDRect rect;
	rect.x = (float)(monitor.x - m_overlay.originX());
	rect.y = (float)(monitor.y - m_overlay.originY());
	rect.width = (float)monitor.width;
	rect.height = (float)monitor.height;

	return rect;
}

void PDMainApplication::applyPendingScene()
{
	std::vector<PDSceneEntry> entries;

	{
		std::lock_guard<std::mutex> const lock(m_sceneMutex);

		if (not m_pendingApply)
		{
			return;
		}

		entries = std::move(m_pendingEntries);
		m_pendingEntries.clear();
		m_pendingApply = false;
	}

	m_scene.clear();

	float x = 0.0f;

	for (PDSceneEntry const& entry : entries)
	{
		for (int i = 0; i < entry.quantity; i += 1)
		{
			m_scene.spawnEntity(entry.packPath, x, 0.0f);
			x += 120.0f;
		}
	}
}

void PDMainApplication::startScene(std::vector<PDSceneEntry> const& entries)
{
	{
		std::lock_guard<std::mutex> const lock(m_sceneMutex);
		m_pendingEntries = entries;
		m_pendingApply = true;
	}

	m_sceneRunning = true;
}

void PDMainApplication::stopScene()
{
	{
		std::lock_guard<std::mutex> const lock(m_sceneMutex);
		m_pendingEntries.clear();
		m_pendingApply = true;
	}

	m_sceneRunning = false;
}

void PDMainApplication::reshuffle()
{
	m_pendingShuffle = true;
}

void PDMainApplication::force(PDForce const& request)
{
	std::lock_guard<std::mutex> const lock(m_sceneMutex);
	m_pendingForces.push_back(request);
}

void PDMainApplication::applyPendingForces()
{
	std::vector<PDForce> requests;

	{
		std::lock_guard<std::mutex> const lock(m_sceneMutex);
		requests = std::move(m_pendingForces);
		m_pendingForces.clear();
	}

	for (PDForce const& request : requests)
	{
		m_scene.force(request);
	}
}

void PDMainApplication::applyPendingPacks()
{
	std::vector<std::string> paths;

	{
		std::lock_guard<std::mutex> const lock(m_packsMutex);

		if (m_pendingPacks.empty())
		{
			return;
		}

		paths = std::move(m_pendingPacks);
		m_pendingPacks.clear();
	}

	for (std::string const& path : paths)
	{
		m_scene.reloadPack(path);
	}
}

void PDMainApplication::reloadPack(std::string const& packPath)
{
	std::lock_guard<std::mutex> const lock(m_packsMutex);

	if (std::find(m_pendingPacks.begin(), m_pendingPacks.end(), packPath) != m_pendingPacks.end())
	{
		return;
	}

	m_pendingPacks.push_back(packPath);
}

void PDMainApplication::requestExit()
{
	m_imgui.close();
}

void PDMainApplication::handleMessage(UINT message, LPARAM lParam)
{
	if (message == WM_DESTROY)
	{
		m_tray.destroy();

		return;
	}

	switch (m_tray.handleMessage(message, lParam, isRunning(), not m_mainWindow.scene().empty()))
	{
		case PDTrayCommand::Show:
		{
			m_imgui.show();

			break;
		}

		case PDTrayCommand::Toggle:
		{
			if (isRunning())
			{
				stopScene();
			}
			else
			{
				startScene(m_mainWindow.scene());
			}

			break;
		}

		case PDTrayCommand::Quit:
		{
			requestExit();

			break;
		}

		case PDTrayCommand::None:
		{
			break;
		}
	}
}
