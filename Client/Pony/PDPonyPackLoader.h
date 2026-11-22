#pragma once

#include <Pony/PDPonyPackData.h>

#include <string>

bool loadPonyPack(std::string const& packPath, PDPonyPackData* result, std::string* error);
