#pragma once

#include <filesystem>
#include <string>

std::filesystem::path nativePath(std::string const& path);
std::string pathString(std::filesystem::path const& path);
std::string packsDir();
