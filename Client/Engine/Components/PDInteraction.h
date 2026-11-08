#pragma once

#include <string>
#include <unordered_map>

struct PDInteraction
{
	bool busy = false;
	std::unordered_map<std::string, float> cooldowns;
	float countdown = 0.0f;
};
