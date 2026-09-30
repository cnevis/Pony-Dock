#include <Pony/PDPonyCatalog.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

static std::string stringOr(nlohmann::json const &object, char const *key, std::string const &fallback)
{
	auto const found = object.find(key);

	return found != object.end() and found->is_string() ? found->get<std::string>() : fallback;
}

void PDPonyCatalog::load(std::string const &packsRoot)
{
	m_groups.clear();

	std::unordered_map<std::string, std::size_t> groupByName;
	std::error_code error;

	for (std::filesystem::directory_iterator it(packsRoot, error), end; it != end and not error; it.increment(error))
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
		catch (nlohmann::json::parse_error const &)
		{
			continue;
		}

		if (not document.is_object())
		{
			continue;
		}

		std::string const id = stringOr(document, "id", std::string());
		std::string const displayName = stringOr(document, "name", id);
		std::string const preview = stringOr(document, "preview", std::string());

		if (id.empty() or id == "random-pony")
		{
			continue;
		}

		PDPonyPack pack;
		pack.id = id;
		pack.packPath = packPath.string();
		pack.previewPath = preview.empty() ? std::string() : (packPath / preview).string();

		auto const existing = groupByName.find(displayName);

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

	std::sort(m_groups.begin(), m_groups.end(), [](PDPonyGroup const &a, PDPonyGroup const &b)
	{
		return a.displayName < b.displayName;
	});
}
