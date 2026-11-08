#include <Engine/PDAnimationLoader.h>

#include <Core/PDPaths.h>

#include <nlohmann/json.hpp>

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

bool loadAnimationClip(std::string const &animPath, PDAnimationClip &outClip, std::string &outError)
{
	outClip = PDAnimationClip();
	outError.clear();

	std::ifstream stream(nativePath(animPath), std::ios::binary);

	if (not stream)
	{
		outError = "cannot open " + animPath;

		return false;
	}

	nlohmann::json document;

	try
	{
		stream >> document;
	}
	catch (nlohmann::json::exception const& error)
	{
		outError = std::string("parse error in ") + animPath + ": " + error.what();

		return false;
	}

	if (not document.is_object())
	{
		outError = animPath + " is not a JSON object";

		return false;
	}

	nlohmann::json::const_iterator const atlasSize = document.find("atlasSize");

	if (atlasSize == document.end() or not atlasSize->is_array() or atlasSize->size() != 2)
	{
		outError = animPath + " has a missing or invalid atlasSize";

		return false;
	}

	if (not finiteNumber((*atlasSize)[0]) or not finiteNumber((*atlasSize)[1]))
	{
		outError = animPath + " has a missing or invalid atlasSize";

		return false;
	}

	float const atlasWidth = (*atlasSize)[0].get<float>();
	float const atlasHeight = (*atlasSize)[1].get<float>();

	if (atlasWidth <= 0.0f or atlasHeight <= 0.0f)
	{
		outError = animPath + " has a non-positive atlasSize";

		return false;
	}

	if (not document.contains("atlas") or not document["atlas"].is_string())
	{
		outError = animPath + " has a missing or invalid atlas";

		return false;
	}

	std::filesystem::path const directory = nativePath(animPath).parent_path();
	outClip.atlasPath = pathString((directory / nativePath(document["atlas"].get<std::string>())).lexically_normal());

	if (not document.contains("frames") or not document["frames"].is_array() or document["frames"].empty())
	{
		outError = animPath + " has no frames";

		return false;
	}

	for (nlohmann::json const &frameValue : document["frames"])
	{
		if (not frameValue.is_object() or not frameValue.contains("rect"))
		{
			outError = animPath + " has a frame with no rect";

			return false;
		}

		nlohmann::json const& rect = frameValue["rect"];

		if (not rect.is_array() or rect.size() != 4)
		{
			outError = animPath + " has a frame with a malformed rect";

			return false;
		}

		for (nlohmann::json const& value : rect)
		{
			if (not finiteNumber(value))
			{
				outError = animPath + " has a frame with an invalid rect number";

				return false;
			}
		}

		float const x = rect[0].get<float>();
		float const y = rect[1].get<float>();
		float const width = rect[2].get<float>();
		float const height = rect[3].get<float>();

		if (width <= 0.0f or height <= 0.0f)
		{
			outError = animPath + " has a frame with a non-positive size";

			return false;
		}

		if (x < 0.0f or y < 0.0f or x + width > atlasWidth or y + height > atlasHeight)
		{
			outError = animPath + " has a frame rect outside the atlas";

			return false;
		}

		float durationSeconds = MinFrameSeconds;

		if (frameValue.contains("durationMs") and frameValue["durationMs"].is_number())
		{
			if (not finiteNumber(frameValue["durationMs"]))
			{
				outError = animPath + " has a frame with an invalid duration";

				return false;
			}

			durationSeconds = frameValue["durationMs"].get<float>() / 1000.0f;
		}

		PDAnimationFrame frame;
		frame.u0 = x / atlasWidth;
		frame.v0 = y / atlasHeight;
		frame.u1 = (x + width) / atlasWidth;
		frame.v1 = (y + height) / atlasHeight;
		frame.width = width;
		frame.height = height;
		frame.durationSeconds = durationSeconds > MinFrameSeconds ? durationSeconds : MinFrameSeconds;

		outClip.frames.push_back(frame);
	}

	outClip.loop = not document.contains("loop") or not document["loop"].is_boolean() or document["loop"].get<bool>();

	float pivotX = 0.0f;
	float pivotY = 0.0f;

	if (document.contains("pivot"))
	{
		nlohmann::json const& pivot = document["pivot"];

		if (not pivot.is_array() or pivot.size() != 2)
		{
			outError = animPath + " has an invalid pivot";

			return false;
		}

		if (not finiteNumber(pivot[0]) or not finiteNumber(pivot[1]))
		{
			outError = animPath + " has an invalid pivot number";

			return false;
		}

		pivotX = pivot[0].get<float>();
		pivotY = pivot[1].get<float>();
	}

	if (pivotX == 0.0f and pivotY == 0.0f)
	{
		pivotX = outClip.frames.front().width * 0.5f;
		pivotY = outClip.frames.front().height * 0.5f;
	}

	outClip.pivotX = pivotX;
	outClip.pivotY = pivotY;
	outClip.valid = true;

	return true;
}
