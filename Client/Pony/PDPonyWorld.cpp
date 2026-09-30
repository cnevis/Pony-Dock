#include <Pony/PDPonyWorld.h>

#include <Engine/PDDiagnostics.h>
#include <Engine/PDScene.h>
#include <Pony/PDPonyPackLoader.h>

#include <filesystem>

static std::string packKey(std::string const &packPath)
{
	return std::filesystem::path(packPath).lexically_normal().string();
}

void PDPonyWorld::initialize(PDScene &scene, PDDiagnostics &diagnostics)
{
	m_scene = &scene;
	m_diagnostics = &diagnostics;
}

PDPonyWorld::LoadedPack *PDPonyWorld::pack(std::string const &packPath)
{
	std::string const key = packKey(packPath);
	auto const existing = m_packs.find(key);

	if (existing != m_packs.end())
	{
		return existing->second.data.valid ? &existing->second : nullptr;
	}

	LoadedPack &loaded = m_packs[key];
	std::string error;

	if (not loadPonyPack(key, loaded.data, error))
	{
		m_diagnostics->writeOnce(key, "Pack load failed: " + error);

		return nullptr;
	}

	publish(loaded);
	m_diagnostics->write("Loaded pack " + loaded.data.id + " (" + std::to_string(loaded.data.animations.size()) + " animations, " + std::to_string(loaded.data.behaviors.size()) + " behaviors)");

	return &loaded;
}

void PDPonyWorld::publish(LoadedPack &loaded)
{
	PDPonyPackData const &data = loaded.data;

	loaded.kind.id = data.id;
	loaded.kind.previewPath = data.previewPath;
	loaded.kind.initialAnimation = data.behaviors.empty() ? std::string() : data.behaviors.front().animation;
	loaded.kind.animations = data.animations;

	if (not loaded.proxy.valid())
	{
		sol::state &lua = m_scene->lua().state();

		loaded.meta = lua.create_table();
		loaded.meta["__metatable"] = false;

		loaded.meta["__newindex"] = [](sol::this_state state, sol::table, sol::object, sol::object)
		{
			return luaL_error(state.lua_state(), "pack data is read-only");
		};

		loaded.proxy = lua.create_table();
		loaded.proxy[sol::metatable_key] = loaded.meta;
	}

	loaded.meta["__index"] = buildTable(data);
}

void PDPonyWorld::spawn(std::string const &packPath, float x, float y)
{
	LoadedPack *const loaded = pack(packPath);

	if (loaded == nullptr)
	{
		return;
	}

	entt::entity const entity = m_scene->spawn(loaded->kind, x, y);
	m_scene->self(entity)["pack"] = loaded->proxy;
}

void PDPonyWorld::reloadPack(std::string const &packPath)
{
	std::string const key = packKey(packPath);
	auto const existing = m_packs.find(key);

	if (existing == m_packs.end())
	{
		return;
	}

	PDPonyPackData loaded;
	std::string error;

	if (not loadPonyPack(key, loaded, error))
	{
		m_diagnostics->write("Pack reload failed: " + error);

		return;
	}

	existing->second.data = std::move(loaded);
	publish(existing->second);
	m_scene->restartScripts(existing->second.kind);

	m_diagnostics->write("Reloaded pack " + existing->second.data.id);
}

sol::table PDPonyWorld::buildTable(PDPonyPackData const &pack)
{
	sol::state &lua = m_scene->lua().state();
	sol::table behaviors = lua.create_table();
	sol::table byId = lua.create_table();

	for (std::size_t index = 0; index < pack.behaviors.size(); index += 1)
	{
		PDPonyBehaviorData const &source = pack.behaviors[index];

		sol::table behavior = lua.create_table();
		behavior["id"] = source.id;
		behavior["name"] = source.name;
		behavior["animation"] = source.animation;
		behavior["movement"] = source.movement;
		behavior["chance"] = source.chance;
		behavior["duration_min"] = source.durationMinSeconds;
		behavior["duration_max"] = source.durationMaxSeconds;
		behavior["speed"] = source.speedPxPerSec;
		behavior["group"] = source.group;
		behavior["special"] = source.special;
		behavior["skip"] = source.skip;
		behavior["prevent_animation_loop"] = source.preventAnimationLoop;

		if (not source.linkedBehavior.empty())
		{
			behavior["linked"] = source.linkedBehavior;
		}

		behaviors[index + 1] = behavior;

		if (not source.id.empty())
		{
			byId[source.id] = behavior;
		}
	}

	sol::table groups = lua.create_table();

	for (std::size_t index = 0; index < pack.groups.size(); index += 1)
	{
		groups[index + 1] = pack.groups[index].id;
	}

	sol::table interactions = lua.create_table();

	for (std::size_t i = 0; i < pack.interactions.size(); i += 1)
	{
		PDPonyInteractionData const &source = pack.interactions[i];

		sol::table targets = lua.create_table();

		for (std::size_t j = 0; j < source.targets.size(); j += 1)
		{
			targets[j + 1] = source.targets[j];
		}

		sol::table behaviorIds = lua.create_table();

		for (std::size_t j = 0; j < source.behaviors.size(); j += 1)
		{
			behaviorIds[j + 1] = source.behaviors[j];
		}

		sol::table interaction = lua.create_table();
		interaction["id"] = source.id;
		interaction["activation"] = source.activation;
		interaction["targets"] = targets;
		interaction["behaviors"] = behaviorIds;
		interaction["chance"] = source.chance;
		interaction["proximity"] = source.proximityPx;
		interaction["delay"] = source.reactivationDelaySeconds;

		interactions[i + 1] = interaction;
	}

	sol::table real = lua.create_table();
	real["id"] = pack.id;
	real["name"] = pack.name;
	real["behaviors"] = behaviors;
	real["by_id"] = byId;
	real["groups"] = groups;
	real["interactions"] = interactions;

	return real;
}
