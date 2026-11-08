#pragma once

enum class PDSpawnPosition
{
	Anywhere,
	Ground,
	Center
};

struct PDSettings
{
	bool movement = true;
	bool flying = true;
	bool extended = true;
	bool interactions = false;
	bool dragging = true;
	bool names = true;
	bool linked = true;
	float speed = 1.0f;
	PDSpawnPosition start = PDSpawnPosition::Anywhere;
};
