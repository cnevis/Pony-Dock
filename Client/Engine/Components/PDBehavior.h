#pragma once

#include <entt/entity/entity.hpp>

#include <cstddef>

enum class PDMotion
{
	Free,
	Follow,
	Watch,
	Chase
};

struct PDBehavior
{
	std::size_t idx = 0;
	int group = 0;
	int linkDepth = 0;
	int monitor = -1;
	float timer = 0.0f;
	float vx = 0.0f;
	float vy = 0.0f;
	PDMotion motion = PDMotion::Free;
	entt::entity follow = entt::null;
	bool active = false;
	bool placed = false;
	bool grabbed = false;
};
