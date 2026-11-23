#include <Pony/PDPonyPackOverride.h>

#include <Core/PDPaths.h>

#include <windows.h>

#include <shlobj.h>

#include <cassert>
#include <filesystem>
#include <fstream>

static std::string packFolderName(std::string const& packPath)
{
	return pathString(nativePath(packPath).lexically_normal().filename());
}

static std::string overrideFolder()
{
	PWSTR folder = nullptr;

	if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder)))
	{
		return "overrides";
	}

	std::filesystem::path path(folder);
	CoTaskMemFree(folder);

	path /= "Pony Dock";
	path /= "overrides";

	return pathString(path);
}

static std::string overridePath(std::string const& packPath)
{
	std::string const folder = packFolderName(packPath);

	if (folder.empty())
	{
		return std::string();
	}

	return pathString(nativePath(overrideFolder()) / nativePath(folder + ".json"));
}

bool ponyPackOverrideExists(std::string const& packPath)
{
	std::string const path = overridePath(packPath);

	if (path.empty())
	{
		return false;
	}

	std::error_code ec;

	return std::filesystem::is_regular_file(nativePath(path), ec);
}

std::string ponyPackDocumentPath(std::string const& packPath)
{
	if (ponyPackOverrideExists(packPath))
	{
		return overridePath(packPath);
	}

	return pathString(nativePath(packPath) / "pony.json");
}

bool writePonyPackOverride(std::string const& packPath, std::string const& text, std::string* error)
{
	assert(error != nullptr);

	std::string const path = overridePath(packPath);

	if (path.empty())
	{
		*error = "cannot derive an override name for " + packPath;

		return false;
	}

	std::filesystem::path const target = nativePath(path);
	std::error_code ec;
	std::filesystem::create_directories(target.parent_path(), ec);

	if (ec)
	{
		*error = "cannot create " + pathString(target.parent_path());

		return false;
	}

	std::filesystem::path temporary = target;
	temporary += ".tmp";

	{
		std::ofstream stream(temporary, std::ios::binary);

		if (not stream)
		{
			*error = "cannot write " + pathString(temporary);

			return false;
		}

		stream << text;
		stream.close();

		if (not stream)
		{
			*error = "cannot write " + pathString(temporary);

			return false;
		}
	}

	std::filesystem::rename(temporary, target, ec);

	if (ec)
	{
		*error = "cannot replace " + pathString(target);

		return false;
	}

	return true;
}

bool removePonyPackOverride(std::string const& packPath, std::string* error)
{
	assert(error != nullptr);

	std::string const path = overridePath(packPath);

	if (path.empty())
	{
		*error = "cannot derive an override name for " + packPath;

		return false;
	}

	std::error_code ec;
	std::filesystem::remove(nativePath(path), ec);

	if (ec)
	{
		*error = "cannot remove " + path;

		return false;
	}

	return true;
}
