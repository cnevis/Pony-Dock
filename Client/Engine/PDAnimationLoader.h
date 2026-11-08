#pragma once

#include <Engine/PDAnimationClip.h>

#include <string>

bool loadAnimationClip(std::string const& animPath, PDAnimationClip* result, std::string* error);
