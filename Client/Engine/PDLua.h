#pragma once

#include <Engine/PDSettingsStore.h>
#include <Math/PDRect.h>

#include <sol/sol.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class PDDiagnostics;

struct PDNeighbor
{
	std::uint32_t id = 0;
	std::string kind;

	float x = 0.0f;
	float y = 0.0f;
	bool busy = false;
};

class PDLua
{
public:
	using PlayHandler = std::function<bool(std::uint32_t, std::string const &, bool, bool)>;
	using FacingHandler = std::function<void(std::uint32_t, bool)>;
	using NeighborHandler = std::function<std::vector<PDNeighbor>(std::uint32_t, float)>;
	using InviteHandler = std::function<bool(std::uint32_t, std::uint32_t, std::string const &)>;

	void initialize(std::string const &scriptsRoot, PDDiagnostics &diagnostics, PDSettingsStore &settings);

	void setPlayHandler(PlayHandler handler);
	void setFacingHandler(FacingHandler handler);
	void setNeighborHandler(NeighborHandler handler);
	void setInviteHandler(InviteHandler handler);

	void beginFrame(float boundsWidth, float boundsHeight, std::vector<PDRect> const &monitors);

	sol::state &state()
	{
		return m_lua;
	}

	sol::table createSelf(std::uint32_t entityId, std::string const &kindId);
	sol::table createInvite(std::uint32_t from, std::string const &behavior);

	bool pressButton(std::string const &moduleKey, std::string const &settingId);

	bool callSpawn(std::string const &scriptPath, sol::table &self);
	bool callTick(std::string const &scriptPath, sol::table &self, float deltaSeconds);

	bool loadScript(std::string const &scriptPath);
	void unloadScript(std::string const &scriptPath);

	void reload();

	std::vector<std::string> loadedScripts() const;
	std::vector<std::string> modules() const;

private:
	struct Module
	{
		sol::environment environment;
		std::string key;
		sol::protected_function spawn;
		sol::protected_function tick;
		bool failed = false;
	};

	Module *module(std::string const &scriptPath);
	bool report(std::string const &scriptPath, sol::protected_function_result const &result);
	std::string modulePath(std::string const &name) const;
	void forgetPackage(std::string const &normalizedPath);

	sol::table createSettingsTable(std::string const &moduleKey);
	bool declareSetting(std::string const &moduleKey, PDSettingDeclaration declaration);
	PDSettingValue settingValue(std::string const &moduleKey, std::string const &kindId, std::string const &id) const;
	sol::object settingObject(sol::this_state state, std::string const &moduleKey, std::string const &kindId, std::string const &id) const;
	void forgetButtons(std::string const &moduleKey);

	sol::state m_lua;
	PDDiagnostics *m_diagnostics = nullptr;
	std::string m_scriptsRoot;
	std::string m_currentModule;

	sol::table m_metatable;
	sol::table m_frame;

	PlayHandler m_playHandler;
	FacingHandler m_facingHandler;
	NeighborHandler m_neighborHandler;
	InviteHandler m_inviteHandler;

	std::unordered_map<std::string, Module> m_modules;
	mutable std::unordered_map<std::string, std::string> m_modulePaths;

	PDSettingsStore *m_settings = nullptr;
	PDSettingsSnapshot m_snapshot;
	std::vector<PDRect> m_monitors;
	std::unordered_map<std::string, sol::protected_function> m_buttonHandlers;
};
