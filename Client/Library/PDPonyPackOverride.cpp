#include <Library/PDPonyPackOverride.h>

#include <Core/PDPaths.h>

#include <windows.h>

#include <shlobj.h>

#include <filesystem>
#include <fstream>

static std::string packFolderName(std::string const &packPath)
{
	return pathString(nativePath(packPath).lexically_normal().filename());
}

std::string ponyPackOverrideRoot()
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

std::string ponyPackOverridePath(std::string const &packPath)
{
	std::string const folder = packFolderName(packPath);

	if (folder.empty())
	{
		return std::string();
	}

	return pathString(nativePath(ponyPackOverrideRoot()) / nativePath(folder + ".json"));
}

bool ponyPackOverrideExists(std::string const &packPath)
{
	std::string const path = ponyPackOverridePath(packPath);

	if (path.empty())
	{
		return false;
	}

	std::error_code error;

	return std::filesystem::is_regular_file(nativePath(path), error);
}

std::string ponyPackDocumentPath(std::string const &packPath)
{
	if (ponyPackOverrideExists(packPath))
	{
		return ponyPackOverridePath(packPath);
	}

	return pathString(nativePath(packPath) / "pony.json");
}

bool writePonyPackOverride(std::string const &packPath, std::string const &text, std::string &outError)
{
	std::string const path = ponyPackOverridePath(packPath);

	if (path.empty())
	{
		outError = "cannot derive an override name for " + packPath;

		return false;
	}

	std::filesystem::path const target = nativePath(path);
	std::error_code error;
	std::filesystem::create_directories(target.parent_path(), error);

	if (error)
	{
		outError = "cannot create " + pathString(target.parent_path());

		return false;
	}

	std::filesystem::path temporary = target;
	temporary += ".tmp";

	{
		std::ofstream stream(temporary, std::ios::binary);

		if (not stream)
		{
			outError = "cannot write " + pathString(temporary);

			return false;
		}

		stream << text;
		stream.close();

		if (not stream)
		{
			outError = "cannot write " + pathString(temporary);

			return false;
		}
	}

	std::filesystem::rename(temporary, target, error);

	if (error)
	{
		outError = "cannot replace " + pathString(target);

		return false;
	}

	return true;
}

bool removePonyPackOverride(std::string const &packPath, std::string &outError)
{
	std::string const path = ponyPackOverridePath(packPath);

	if (path.empty())
	{
		outError = "cannot derive an override name for " + packPath;

		return false;
	}

	std::error_code error;
	std::filesystem::remove(nativePath(path), error);

	if (error)
	{
		outError = "cannot remove " + path;

		return false;
	}

	return true;
}
