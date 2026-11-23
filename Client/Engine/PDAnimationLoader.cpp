#include <Engine/PDAnimationLoader.h>

#include <Core/PDPaths.h>

#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

static constexpr float MinFrameSeconds = 0.001f;

static bool finiteNumber(nlohmann::json const& value)
{
	if (not value.is_number())
	{
		return false;
	}

	double const number = value.get<double>();

	return std::isfinite(number)
		and number >= -std::numeric_limits<float>::max()
		and number <= std::numeric_limits<float>::max();
}

static bool parseFrame(
	nlohmann::json const& value,
	std::string const& animPath,
	float atlasWidth,
	float atlasHeight,
	PDAnimationFrame* result,
	std::string* error)
{
	if (not value.is_object() or not value.contains("rect"))
	{
		*error = animPath + " has a frame with no rect";

		return false;
	}

	nlohmann::json const& rect = value["rect"];

	if (not rect.is_array() or rect.size() != 4)
	{
		*error = animPath + " has a frame with a malformed rect";

		return false;
	}

	for (nlohmann::json const& number : rect)
	{
		if (not finiteNumber(number))
		{
			*error = animPath + " has a frame with an invalid rect number";

			return false;
		}
	}

	float const x = rect[0].get<float>();
	float const y = rect[1].get<float>();
	float const width = rect[2].get<float>();
	float const height = rect[3].get<float>();

	if (width <= 0.0f or height <= 0.0f)
	{
		*error = animPath + " has a frame with a non-positive size";

		return false;
	}

	if (x < 0.0f or y < 0.0f or x + width > atlasWidth or y + height > atlasHeight)
	{
		*error = animPath + " has a frame rect outside the atlas";

		return false;
	}

	float durationSeconds = MinFrameSeconds;

	if (value.contains("durationMs") and value["durationMs"].is_number())
	{
		if (not finiteNumber(value["durationMs"]))
		{
			*error = animPath + " has a frame with an invalid duration";

			return false;
		}

		durationSeconds = value["durationMs"].get<float>() / 1000.0f;
	}

	result->u0 = x / atlasWidth;
	result->v0 = y / atlasHeight;
	result->u1 = (x + width) / atlasWidth;
	result->v1 = (y + height) / atlasHeight;
	result->width = width;
	result->height = height;
	result->durationSeconds = durationSeconds > MinFrameSeconds ? durationSeconds : MinFrameSeconds;

	return true;
}

bool loadAnimationClip(std::string const& animPath, PDAnimationClip* result, std::string* error)
{
	assert(result != nullptr);
	assert(error != nullptr);

	*result = PDAnimationClip();
	error->clear();

	std::ifstream stream(nativePath(animPath), std::ios::binary);

	if (not stream)
	{
		*error = "cannot open " + animPath;

		return false;
	}

	nlohmann::json document;

	try
	{
		stream >> document;
	}
	catch (nlohmann::json::exception const& e)
	{
		*error = std::string("parse error in ") + animPath + ": " + e.what();

		return false;
	}

	if (not document.is_object())
	{
		*error = animPath + " is not a JSON object";

		return false;
	}

	nlohmann::json::const_iterator const atlasSize = document.find("atlasSize");

	if (atlasSize == document.end() or not atlasSize->is_array() or atlasSize->size() != 2)
	{
		*error = animPath + " has a missing or invalid atlasSize";

		return false;
	}

	if (not finiteNumber((*atlasSize)[0]) or not finiteNumber((*atlasSize)[1]))
	{
		*error = animPath + " has a missing or invalid atlasSize";

		return false;
	}

	float const atlasWidth = (*atlasSize)[0].get<float>();
	float const atlasHeight = (*atlasSize)[1].get<float>();

	if (atlasWidth <= 0.0f or atlasHeight <= 0.0f)
	{
		*error = animPath + " has a non-positive atlasSize";

		return false;
	}

	if (not document.contains("atlas") or not document["atlas"].is_string())
	{
		*error = animPath + " has a missing or invalid atlas";

		return false;
	}

	std::filesystem::path const directory = nativePath(animPath).parent_path();
	result->atlasPath = pathString((directory / nativePath(document["atlas"].get<std::string>())).lexically_normal());

	if (not document.contains("frames") or not document["frames"].is_array() or document["frames"].empty())
	{
		*error = animPath + " has no frames";

		return false;
	}

	for (nlohmann::json const& value : document["frames"])
	{
		PDAnimationFrame frame;

		if (not parseFrame(value, animPath, atlasWidth, atlasHeight, &frame, error))
		{
			return false;
		}

		result->frames.push_back(frame);
	}

	float pivotX = 0.0f;
	float pivotY = 0.0f;

	if (document.contains("pivot"))
	{
		nlohmann::json const& pivot = document["pivot"];

		if (not pivot.is_array() or pivot.size() != 2)
		{
			*error = animPath + " has an invalid pivot";

			return false;
		}

		if (not finiteNumber(pivot[0]) or not finiteNumber(pivot[1]))
		{
			*error = animPath + " has an invalid pivot number";

			return false;
		}

		pivotX = pivot[0].get<float>();
		pivotY = pivot[1].get<float>();
	}

	if (pivotX == 0.0f and pivotY == 0.0f)
	{
		pivotX = result->frames.front().width * 0.5f;
		pivotY = result->frames.front().height * 0.5f;
	}

	result->pivotX = pivotX;
	result->pivotY = pivotY;
	result->valid = true;

	return true;
}
