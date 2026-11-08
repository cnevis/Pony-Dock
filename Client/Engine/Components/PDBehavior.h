#pragma once

#include <cstddef>

struct PDBehavior
{
	std::size_t index = 0;
	int group = 0;
	int linkDepth = 0;
	int monitor = -1;
	float timer = 0.0f;
	float vx = 0.0f;
	float vy = 0.0f;
	bool active = false;
	bool placed = false;
	bool grabbed = false;
};
