#pragma once

#include <string>
#include <unordered_map>

struct PDAnimationRef
{
	std::string left;
	std::string right;
};

struct PDEntityKind
{
	std::string id;
	std::string previewPath;
	std::string initialAnimation;

	std::unordered_map<std::string, PDAnimationRef> animations;
};
