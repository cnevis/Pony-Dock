#pragma once

#include <Engine/PDEntityKind.h>
#include <Pony/PDPonyPackData.h>

#include <sol/sol.hpp>

#include <string>
#include <unordered_map>

class PDDiagnostics;
class PDScene;

class PDPonyWorld
{
public:
	void initialize(PDScene &scene, PDDiagnostics &diagnostics);

	void spawn(std::string const &packPath, float x, float y);
	void reloadPack(std::string const &packPath);

private:
	struct LoadedPack
	{
		PDPonyPackData data;
		PDEntityKind kind;
		sol::table proxy;
		sol::table meta;
	};

	LoadedPack *pack(std::string const &packPath);
	void publish(LoadedPack &loaded);
	sol::table buildTable(PDPonyPackData const &data);

	PDScene *m_scene = nullptr;
	PDDiagnostics *m_diagnostics = nullptr;
	std::unordered_map<std::string, LoadedPack> m_packs;
};
