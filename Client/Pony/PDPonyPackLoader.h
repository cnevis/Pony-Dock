#pragma once

#include <Pony/PDPonyPackData.h>

#include <string>

bool loadPonyPack(std::string const &packPath, PDPonyPackData &outData, std::string &outError);
