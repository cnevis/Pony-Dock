#include <Library/PDPonyCatalog.h>

#include <Core/PDPaths.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

void PDPonyCatalog::load(std::string const& packsRoot)
{
	m_groups.clear();

	std::unordered_map<std::string, std::size_t> groupByName;
	std::error_code error;

	for (std::filesystem::directory_iterator it(nativePath(packsRoot), error), end; it != end and not error; it.increment(error))
	{
		std::filesystem::path const packPath = it->path();
		std::filesystem::path const jsonPath = packPath / "pony.json";

		std::ifstream stream(jsonPath, std::ios::binary);

		if (not stream)
		{
			continue;
		}

		nlohmann::json document;

		try
		{
			stream >> document;
		}
		catch (nlohmann::json::exception const&)
		{
			continue;
		}

		if (not document.is_object())
		{
			continue;
		}

		nlohmann::json::const_iterator const idNode = document.find("id");
		nlohmann::json::const_iterator const nameNode = document.find("name");
		nlohmann::json::const_iterator const previewNode = document.find("preview");

		if (idNode == document.end() or not idNode->is_string())
		{
			continue;
		}

		std::string const id = idNode->get<std::string>();
		std::string const displayName = nameNode != document.end() and nameNode->is_string() ? nameNode->get<std::string>() : id;
		std::string const preview = previewNode != document.end() and previewNode->is_string() ? previewNode->get<std::string>() : std::string();

		if (id.empty() or id == "random-pony")
		{
			continue;
		}

		PDPonyPack pack;
		pack.id = id;
		pack.packPath = pathString(packPath);
		pack.previewPath = preview.empty() ? std::string() : pathString(packPath / nativePath(preview));

		std::unordered_map<std::string, std::size_t>::iterator const existing = groupByName.find(displayName);

		if (existing == groupByName.end())
		{
			groupByName.emplace(displayName, m_groups.size());

			PDPonyGroup group;
			group.displayName = displayName;
			group.variants.push_back(std::move(pack));
			m_groups.push_back(std::move(group));
		}
		else
		{
			m_groups[existing->second].variants.push_back(std::move(pack));
		}
	}

	std::sort(m_groups.begin(), m_groups.end(), [](PDPonyGroup const& a, PDPonyGroup const& b)
	{
		return a.displayName < b.displayName;
	});
}
