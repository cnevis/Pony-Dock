#pragma once

#include <Library/PDPonyPackData.h>

#include <string>

bool loadPonyPack(std::string const& packPath, PDPonyPackData* result, std::string* error);
