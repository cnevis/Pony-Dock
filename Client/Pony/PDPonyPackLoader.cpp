#include <Pony/PDPonyPackLoader.h>

#include <Core/PDPaths.h>
#include <Pony/PDPonyPackOverride.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

static std::string stringOr(nlohmann::json const& object, char const* key, std::string const& fallback)
{
	nlohmann::json::const_iterator const found = object.find(key);

	return found != object.end() and found->is_string() ? found->get<std::string>() : fallback;
}

static float numberOr(nlohmann::json const& object, char const* key, float fallback)
{
	nlohmann::json::const_iterator const found = object.find(key);

	if (found == object.end() or not found->is_number())
	{
		return fallback;
	}

	double const value = found->get<double>();

	if (not std::isfinite(value) or value < -std::numeric_limits<float>::max() or value > std::numeric_limits<float>::max())
	{
		return fallback;
	}

	return (float)value;
}

static int intOr(nlohmann::json const& object, char const* key, int fallback)
{
	nlohmann::json::const_iterator const found = object.find(key);

	if (found == object.end() or not found->is_number_integer())
	{
		return fallback;
	}

	double const value = found->get<double>();

	if (value < std::numeric_limits<int>::min() or value > std::numeric_limits<int>::max())
	{
		return fallback;
	}

	return found->get<int>();
}

static bool boolOr(nlohmann::json const& object, char const* key, bool fallback)
{
	nlohmann::json::const_iterator const found = object.find(key);

	return found != object.end() and found->is_boolean() ? found->get<bool>() : fallback;
}

static std::vector<std::string> stringArray(nlohmann::json const& object, char const* key)
{
	std::vector<std::string> values;
	nlohmann::json::const_iterator const found = object.find(key);

	if (found == object.end() or not found->is_array())
	{
		return values;
	}

	for (nlohmann::json const& entry : *found)
	{
		if (entry.is_string())
		{
			values.push_back(entry.get<std::string>());
		}
	}

	return values;
}

static std::string packId(std::string const& name)
{
	std::string id;
	bool dash = false;

	for (char c : name)
	{
		if (c >= 'A' and c <= 'Z')
		{
			c = (char)(c + ('a' - 'A'));
		}

		if ((c >= 'a' and c <= 'z') or (c >= '0' and c <= '9'))
		{
			if (dash and not id.empty())
			{
				id += '-';
			}

			id += c;
			dash = false;
		}
		else
		{
			dash = true;
		}
	}

	return id;
}

static std::string absolutePath(std::filesystem::path const& packPath, std::string const& relative)
{
	if (relative.empty())
	{
		return std::string();
	}

	return pathString((packPath / nativePath(relative)).lexically_normal());
}

static bool groupHasBehavior(PDPonyPackData const& data, int group)
{
	for (PDPonyBehaviorData const& behavior : data.behaviors)
	{
		if (behavior.group == group)
		{
			return true;
		}
	}

	return false;
}

static bool groupIsKnown(PDPonyPackData const& data, int group)
{
	for (PDPonyBehaviorGroup const& entry : data.groups)
	{
		if (entry.id == group)
		{
			return true;
		}
	}

	return false;
}

static void readBehaviorGroups(nlohmann::json const& document, PDPonyPackData* data)
{
	nlohmann::json::const_iterator const declared = document.find("behaviorGroups");

	if (declared != document.end() and declared->is_array())
	{
		for (nlohmann::json const& entry : *declared)
		{
			if (not entry.is_object())
			{
				continue;
			}

			PDPonyBehaviorGroup group;
			group.id = intOr(entry, "id", 0);
			group.name = stringOr(entry, "name", std::string());

			if (group.id == 0 or groupIsKnown(*data, group.id) or not groupHasBehavior(*data, group.id))
			{
				continue;
			}

			data->groups.push_back(std::move(group));
		}
	}

	std::size_t const declaredCount = data->groups.size();

	for (PDPonyBehaviorData const& behavior : data->behaviors)
	{
		if (behavior.group == 0 or groupIsKnown(*data, behavior.group))
		{
			continue;
		}

		PDPonyBehaviorGroup group;
		group.id = behavior.group;
		data->groups.push_back(std::move(group));
	}

	std::sort(
		data->groups.begin() + (std::ptrdiff_t)declaredCount,
		data->groups.end(),
		[](PDPonyBehaviorGroup const& a, PDPonyBehaviorGroup const& b)
	{
		return a.id < b.id;
	});
}

static void readInteractions(nlohmann::json const& document, PDPonyPackData* data)
{
	nlohmann::json::const_iterator const interactions = document.find("interactions");

	if (interactions == document.end() or not interactions->is_array())
	{
		return;
	}

	for (nlohmann::json const& entry : *interactions)
	{
		if (not entry.is_object())
		{
			continue;
		}

		PDPonyInteractionData interaction;
		interaction.id = stringOr(entry, "id", std::string());
		interaction.activation = stringOr(entry, "activation", "One");
		interaction.targets = stringArray(entry, "targets");
		interaction.behaviors = stringArray(entry, "behaviors");
		interaction.chance = numberOr(entry, "chance", 0.0f);
		interaction.proximityPx = numberOr(entry, "proximityPx", 0.0f);
		interaction.reactivationDelaySeconds = numberOr(entry, "reactivationDelayMs", 0.0f) / 1000.0f;

		if (interaction.id.empty() or interaction.targets.empty() or interaction.behaviors.empty())
		{
			continue;
		}

		data->interactions.push_back(std::move(interaction));
	}
}

static void readAnimations(nlohmann::json const& document, std::filesystem::path const& folder, PDPonyPackData* data)
{
	nlohmann::json::const_iterator const animations = document.find("animations");

	if (animations == document.end() or not animations->is_object())
	{
		return;
	}

	for (nlohmann::json::const_iterator it = animations->begin(); it != animations->end(); ++it)
	{
		if (not it.value().is_object())
		{
			continue;
		}

		PDPonyAnimationRef reference;
		reference.left = absolutePath(folder, stringOr(it.value(), "left", std::string()));
		reference.right = absolutePath(folder, stringOr(it.value(), "right", std::string()));

		if (reference.left.empty() and reference.right.empty())
		{
			continue;
		}

		if (reference.left.empty())
		{
			reference.left = reference.right;
		}

		if (reference.right.empty())
		{
			reference.right = reference.left;
		}

		data->animations.emplace(it.key(), std::move(reference));
	}
}

static PDPonyBehaviorData readBehavior(nlohmann::json const& entry)
{
	PDPonyBehaviorData behavior;
	behavior.id = stringOr(entry, "id", std::string());
	behavior.name = stringOr(entry, "name", behavior.id);
	behavior.animation = stringOr(entry, "animation", std::string());
	behavior.movement = stringOr(entry, "movement", "None");
	behavior.linkedBehavior = stringOr(entry, "linkedBehavior", std::string());
	behavior.chance = numberOr(entry, "chance", 0.0f);
	behavior.speedPxPerSec = numberOr(entry, "speedPxPerSec", 0.0f);
	behavior.group = intOr(entry, "group", 0);
	behavior.special = boolOr(entry, "special", false);
	behavior.skip = boolOr(entry, "skip", false);
	behavior.preventAnimationLoop = boolOr(entry, "preventAnimationLoop", false);

	nlohmann::json::const_iterator const duration = entry.find("durationMs");

	if (duration != entry.end() and duration->is_object())
	{
		behavior.durationMinSeconds = numberOr(*duration, "min", 0.0f) / 1000.0f;
		behavior.durationMaxSeconds = numberOr(*duration, "max", 0.0f) / 1000.0f;
	}

	if (behavior.durationMaxSeconds < behavior.durationMinSeconds)
	{
		behavior.durationMaxSeconds = behavior.durationMinSeconds;
	}

	nlohmann::json::const_iterator const follow = entry.find("follow");

	if (follow != entry.end() and follow->is_object())
	{
		behavior.followTarget = packId(stringOr(*follow, "target", std::string()));
		behavior.followMoving = stringOr(*follow, "movingBehavior", std::string());
		behavior.followStopped = stringOr(*follow, "stoppedBehavior", std::string());
		behavior.followOffsetX = numberOr(*follow, "offsetX", 0.0f);
		behavior.followOffsetY = numberOr(*follow, "offsetY", 0.0f);
		behavior.followMirror = boolOr(*follow, "mirrorOffset", false);
		behavior.followAutoImages = boolOr(*follow, "autoSelectImages", true);
	}

	return behavior;
}

static void readBehaviors(nlohmann::json const& document, PDPonyPackData* data)
{
	nlohmann::json::const_iterator const behaviors = document.find("behaviors");

	if (behaviors == document.end() or not behaviors->is_array())
	{
		return;
	}

	for (nlohmann::json const& entry : *behaviors)
	{
		if (not entry.is_object())
		{
			continue;
		}

		PDPonyBehaviorData behavior = readBehavior(entry);

		if (behavior.animation.empty() or data->animations.find(behavior.animation) == data->animations.end())
		{
			continue;
		}

		data->behaviors.push_back(std::move(behavior));
	}
}

bool loadPonyPack(std::string const& packPath, PDPonyPackData* result, std::string* error)
{
	assert(result != nullptr);
	assert(error != nullptr);

	*result = PDPonyPackData();

	std::filesystem::path const folder = nativePath(packPath);
	std::filesystem::path const jsonPath = nativePath(ponyPackDocumentPath(packPath));

	std::ifstream stream(jsonPath, std::ios::binary);

	if (not stream)
	{
		*error = "cannot open " + pathString(jsonPath);

		return false;
	}

	nlohmann::json document;

	try
	{
		stream >> document;
	}
	catch (nlohmann::json::exception const& e)
	{
		*error = std::string("parse error in ") + pathString(jsonPath) + ": " + e.what();

		return false;
	}

	if (not document.is_object())
	{
		*error = pathString(jsonPath) + " is not a JSON object";

		return false;
	}

	result->packPath = pathString(folder.lexically_normal());
	result->id = stringOr(document, "id", pathString(folder.filename()));
	result->name = stringOr(document, "name", result->id);
	result->previewPath = absolutePath(folder, stringOr(document, "preview", std::string()));

	readAnimations(document, folder, result);

	if (result->animations.empty())
	{
		*error = pathString(jsonPath) + " has no usable animations";

		return false;
	}

	readBehaviors(document, result);

	if (result->behaviors.empty())
	{
		*error = pathString(jsonPath) + " has no usable behaviors";

		return false;
	}

	readBehaviorGroups(document, result);
	readInteractions(document, result);

	result->valid = true;

	return true;
}
