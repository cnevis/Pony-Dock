#include <Engine/PDSettingsStore.h>

#include <Core/PDPaths.h>

#include <windows.h>

#include <shlobj.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>

static std::string resolveSettingsPath()
{
	PWSTR folder = nullptr;

	if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder)))
	{
		return "settings.json";
	}

	std::filesystem::path path(folder);
	CoTaskMemFree(folder);

	path /= "Pony Dock";
	path /= "settings.json";

	return pathString(path);
}

static void sanitizeSettings(PDSettings* value)
{
	assert(value != nullptr);

	if (not std::isfinite(value->speed))
	{
		value->speed = 1.0f;
	}

	value->speed = std::clamp(value->speed, 0.1f, 4.0f);

	switch (value->start)
	{
		case PDSpawnPosition::Anywhere:
		{
			break;
		}

		case PDSpawnPosition::Ground:
		{
			break;
		}

		case PDSpawnPosition::Center:
		{
			break;
		}

		default:
		{
			value->start = PDSpawnPosition::Anywhere;

			break;
		}
	}
}

static PDSettings readSettings(nlohmann::json const& node, PDSettings const& fallback)
{
	PDSettings value = fallback;
	double const speed = node.value("speed", (double)value.speed);
	std::string const start = node.value("start", std::string());

	value.movement = node.value("movement", value.movement);
	value.flying = node.value("flying", value.flying);
	value.extended = node.value("extended", value.extended);
	value.interactions = node.value("interactions", value.interactions);
	value.chasing = node.value("chasing", value.chasing);
	value.dragging = node.value("dragging", value.dragging);
	value.names = node.value("names", value.names);
	value.linked = node.value("linked", value.linked);
	value.speed = std::isfinite(speed) ? (float)std::clamp(speed, 0.1, 4.0) : 1.0f;

	if (start == "Ground")
	{
		value.start = PDSpawnPosition::Ground;
	}
	else if (start == "Center")
	{
		value.start = PDSpawnPosition::Center;
	}
	else if (not start.empty())
	{
		value.start = PDSpawnPosition::Anywhere;
	}

	sanitizeSettings(&value);

	return value;
}

static nlohmann::json writeSettings(PDSettings const& value)
{
	nlohmann::json node = nlohmann::json::object();
	std::string start = "Anywhere";

	switch (value.start)
	{
		case PDSpawnPosition::Ground:
		{
			start = "Ground";

			break;
		}

		case PDSpawnPosition::Center:
		{
			start = "Center";

			break;
		}

		default:
		{
			break;
		}
	}

	node["movement"] = value.movement;
	node["flying"] = value.flying;
	node["extended"] = value.extended;
	node["interactions"] = value.interactions;
	node["chasing"] = value.chasing;
	node["dragging"] = value.dragging;
	node["names"] = value.names;
	node["linked"] = value.linked;
	node["speed"] = value.speed;
	node["start"] = start;

	return node;
}

static bool readLegacySettings(nlohmann::json const& modules, PDSettingsSnapshot* snapshot)
{
	assert(snapshot != nullptr);

	nlohmann::json const core = modules.value("core.lua", nlohmann::json::object());
	nlohmann::json const interactions = modules.value("interactions.lua", nlohmann::json::object());
	nlohmann::json const coreValues = core.value("settings", nlohmann::json::object());
	nlohmann::json const interactionValues = interactions.value("settings", nlohmann::json::object());
	nlohmann::json const corePacks = core.value("packs", nlohmann::json::object());
	nlohmann::json const interactionPacks = interactions.value("packs", nlohmann::json::object());

	if (not corePacks.is_object() or not interactionPacks.is_object())
	{
		return false;
	}

	snapshot->defaults = readSettings(coreValues, snapshot->defaults);
	snapshot->defaults.interactions = interactionValues.value("enabled", snapshot->defaults.interactions);

	for (nlohmann::json::const_iterator it = corePacks.begin(); it != corePacks.end(); ++it)
	{
		if (it.key().empty())
		{
			continue;
		}

		if (not it->contains("speed")
			and not it->contains("flying")
			and not it->contains("extended")
			and not it->contains("start")
			and not it->contains("names"))
		{
			continue;
		}

		snapshot->packs[it.key()] = readSettings(it.value(), snapshot->defaults);
	}

	for (nlohmann::json::const_iterator it = interactionPacks.begin(); it != interactionPacks.end(); ++it)
	{
		if (it.key().empty())
		{
			continue;
		}

		if (not it->contains("enabled"))
		{
			continue;
		}

		std::unordered_map<std::string, PDSettings>::iterator const existing = snapshot->packs.find(it.key());
		PDSettings value = existing == snapshot->packs.end() ? snapshot->defaults : existing->second;
		value.interactions = it.value().value("enabled", value.interactions);
		snapshot->packs[it.key()] = value;
	}

	return true;
}

static std::vector<PDSceneEntry> readScene(nlohmann::json const& nodes)
{
	std::vector<PDSceneEntry> scene;

	if (not nodes.is_array())
	{
		return scene;
	}

	for (nlohmann::json const& node : nodes)
	{
		if (not node.is_object())
		{
			continue;
		}

		PDSceneEntry entry;
		entry.id = node.value("id", std::string());
		entry.quantity = std::clamp(node.value("quantity", 1), 1, 99);

		if (not entry.id.empty())
		{
			scene.push_back(std::move(entry));
		}
	}

	return scene;
}

static bool replaceFile(std::filesystem::path const& target, std::string const& text)
{
	std::error_code error;

	if (not target.parent_path().empty())
	{
		std::filesystem::create_directories(target.parent_path(), error);

		if (error)
		{
			return false;
		}
	}

	std::filesystem::path temporary = target;
	temporary += ".tmp";

	{
		std::ofstream stream(temporary, std::ios::binary);

		if (not stream)
		{
			return false;
		}

		stream << text;
		stream.close();

		if (not stream)
		{
			return false;
		}
	}

	std::filesystem::rename(temporary, target, error);

	return not error;
}

PDSettingsStore::PDSettingsStore()
	: m_path(resolveSettingsPath())
{
}

bool PDSettingsStore::load()
{
	return read(m_path);
}

bool PDSettingsStore::save() const
{
	return write(m_path);
}

bool PDSettingsStore::exportFile(std::string const& path) const
{
	return write(path);
}

bool PDSettingsStore::importFile(std::string const& path)
{
	PDSettingsStore source;

	if (not source.read(path))
	{
		return false;
	}

	if (not source.write(m_path))
	{
		return false;
	}

	std::unique_lock<std::shared_mutex> const lock(m_mutex);
	m_defaults = source.m_defaults;
	m_packs = std::move(source.m_packs);
	m_app = std::move(source.m_app);
	m_scene = std::move(source.m_scene);
	m_version.fetch_add(1, std::memory_order_release);

	return true;
}

PDSettings PDSettingsStore::value(std::string const& packId) const
{
	std::shared_lock<std::shared_mutex> const lock(m_mutex);
	std::unordered_map<std::string, PDSettings>::const_iterator const existing = m_packs.find(packId);

	return existing == m_packs.end() ? m_defaults : existing->second;
}

void PDSettingsStore::setValue(std::string const& packId, PDSettings const& value)
{
	PDSettings copy = value;
	sanitizeSettings(&copy);

	std::unique_lock<std::shared_mutex> const lock(m_mutex);

	if (packId.empty())
	{
		m_defaults = copy;
	}
	else
	{
		m_packs[packId] = copy;
	}

	m_version.fetch_add(1, std::memory_order_release);
}

bool PDSettingsStore::hasOverride(std::string const& packId) const
{
	std::shared_lock<std::shared_mutex> const lock(m_mutex);

	return m_packs.find(packId) != m_packs.end();
}

void PDSettingsStore::clearOverride(std::string const& packId)
{
	std::unique_lock<std::shared_mutex> const lock(m_mutex);

	if (m_packs.erase(packId) != 0)
	{
		m_version.fetch_add(1, std::memory_order_release);
	}
}

std::vector<std::string> PDSettingsStore::packsWithOverrides() const
{
	std::shared_lock<std::shared_mutex> const lock(m_mutex);
	std::vector<std::string> result;
	result.reserve(m_packs.size());

	for (std::pair<std::string const, PDSettings> const& entry : m_packs)
	{
		result.push_back(entry.first);
	}

	std::sort(result.begin(), result.end());

	return result;
}

bool PDSettingsStore::refresh(PDSettingsSnapshot* snapshot) const
{
	assert(snapshot != nullptr);

	if (snapshot->version == m_version.load(std::memory_order_acquire))
	{
		return false;
	}

	std::shared_lock<std::shared_mutex> const lock(m_mutex);
	snapshot->defaults = m_defaults;
	snapshot->packs = m_packs;
	snapshot->version = m_version.load(std::memory_order_relaxed);

	return true;
}

bool PDSettingsStore::appFlag(std::string const& id, bool fallback) const
{
	std::shared_lock<std::shared_mutex> const lock(m_mutex);
	std::unordered_map<std::string, bool>::const_iterator const existing = m_app.find(id);

	return existing == m_app.end() ? fallback : existing->second;
}

void PDSettingsStore::setAppFlag(std::string const& id, bool value)
{
	std::unique_lock<std::shared_mutex> const lock(m_mutex);
	m_app[id] = value;
	m_version.fetch_add(1, std::memory_order_release);
}

std::vector<PDSceneEntry> PDSettingsStore::scene() const
{
	std::shared_lock<std::shared_mutex> const lock(m_mutex);

	return m_scene;
}

void PDSettingsStore::setScene(std::vector<PDSceneEntry> const& entries)
{
	std::unique_lock<std::shared_mutex> const lock(m_mutex);
	m_scene = entries;
}

bool PDSettingsStore::read(std::string const& path)
{
	std::ifstream stream(nativePath(path), std::ios::binary);

	if (not stream)
	{
		return false;
	}

	nlohmann::json const document = nlohmann::json::parse(stream, nullptr, false);

	if (document.is_discarded() or not document.is_object() or stream.bad())
	{
		return false;
	}

	PDSettingsSnapshot snapshot;
	std::unordered_map<std::string, bool> app;
	std::vector<PDSceneEntry> scene;

	try
	{
		scene = readScene(document.value("scene", nlohmann::json::array()));

		nlohmann::json const flags = document.value("app", nlohmann::json::object());

		if (not flags.is_object())
		{
			return false;
		}

		for (nlohmann::json::const_iterator it = flags.begin(); it != flags.end(); ++it)
		{
			if (it->is_boolean())
			{
				app[it.key()] = it->get<bool>();
			}
		}

		if (document.contains("modules") and not document.contains("defaults") and not document.contains("packs"))
		{
			if (not readLegacySettings(document.at("modules"), &snapshot))
			{
				return false;
			}
		}
		else
		{
			nlohmann::json const defaults = document.value("defaults", nlohmann::json::object());
			nlohmann::json const packs = document.value("packs", nlohmann::json::object());

			if (not packs.is_object())
			{
				return false;
			}

			snapshot.defaults = readSettings(defaults, snapshot.defaults);

			for (nlohmann::json::const_iterator it = packs.begin(); it != packs.end(); ++it)
			{
				if (it.key().empty())
				{
					continue;
				}

				snapshot.packs[it.key()] = readSettings(it.value(), snapshot.defaults);
			}
		}
	}
	catch (nlohmann::json::exception const&)
	{
		return false;
	}

	std::unique_lock<std::shared_mutex> const lock(m_mutex);
	m_defaults = snapshot.defaults;
	m_packs = std::move(snapshot.packs);
	m_app = std::move(app);
	m_scene = std::move(scene);
	m_version.fetch_add(1, std::memory_order_release);

	return true;
}

bool PDSettingsStore::write(std::string const& path) const
{
	nlohmann::json document;

	{
		std::shared_lock<std::shared_mutex> const lock(m_mutex);
		nlohmann::json packs = nlohmann::json::object();
		nlohmann::json scene = nlohmann::json::array();

		for (std::pair<std::string const, PDSettings> const& entry : m_packs)
		{
			packs[entry.first] = writeSettings(entry.second);
		}

		for (PDSceneEntry const& entry : m_scene)
		{
			scene.push_back({{"id", entry.id}, {"quantity", entry.quantity}});
		}

		document["version"] = 2;
		document["defaults"] = writeSettings(m_defaults);
		document["packs"] = std::move(packs);
		document["app"] = m_app;
		document["scene"] = std::move(scene);
	}

	std::string text;

	try
	{
		text = document.dump(2);
	}
	catch (nlohmann::json::exception const&)
	{
		return false;
	}

	return replaceFile(nativePath(path), text);
}
