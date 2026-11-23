#pragma once

#include <Engine/PDSettings.h>
#include <Pony/PDSceneEntry.h>

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct PDSettingsSnapshot
{
	std::uint64_t version = 0;
	PDSettings defaults;
	std::unordered_map<std::string, PDSettings> packs;
};

class PDSettingsStore
{
public:
	PDSettingsStore();

	bool load();
	bool save() const;

	bool exportFile(std::string const& path) const;
	bool importFile(std::string const& path);

	std::string const& path() const
	{
		return m_path;
	}

	PDSettings value(std::string const& packId = {}) const;
	void setValue(std::string const& packId, PDSettings const& value);
	bool hasOverride(std::string const& packId) const;
	void clearOverride(std::string const& packId);
	std::vector<std::string> packsWithOverrides() const;
	bool refresh(PDSettingsSnapshot* snapshot) const;

	bool appFlag(std::string const& id, bool fallback) const;
	void setAppFlag(std::string const& id, bool value);

	std::vector<PDSceneEntry> scene() const;
	void setScene(std::vector<PDSceneEntry> const& entries);

private:
	bool read(std::string const& path);
	bool write(std::string const& path) const;

	mutable std::shared_mutex m_mutex;
	std::atomic<std::uint64_t> m_version = 1;
	std::string m_path;
	PDSettings m_defaults;
	std::unordered_map<std::string, PDSettings> m_packs;
	std::unordered_map<std::string, bool> m_app;
	std::vector<PDSceneEntry> m_scene;
};
