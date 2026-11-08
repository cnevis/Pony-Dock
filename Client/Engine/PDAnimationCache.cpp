#include <Engine/PDAnimationCache.h>

#include <Core/PDPaths.h>
#include <Engine/PDAnimationLoader.h>
#include <Engine/PDDiagnostics.h>
#include <Engine/PDTexture.h>

#include <filesystem>

void PDAnimationCache::initialize(PDTextureCache& textures, PDDiagnostics& diagnostics)
{
	m_textures = &textures;
	m_diagnostics = &diagnostics;
}

PDAnimationClip const* PDAnimationCache::clip(std::string const& animPath)
{
	std::string const key = pathString(nativePath(animPath).lexically_normal());

	std::unordered_map<std::string, PDAnimationClip>::iterator const existing = m_clips.find(key);

	if (existing != m_clips.end())
	{
		return existing->second.valid ? &existing->second : nullptr;
	}

	PDAnimationClip loaded;
	std::string error;

	if (not loadAnimationClip(key, &loaded, &error))
	{
		m_diagnostics->writeOnce(key, "Animation load failed: " + error);
		m_clips.emplace(key, PDAnimationClip());

		return nullptr;
	}

	loaded.atlas = m_textures->texture(loaded.atlasPath);

	if (loaded.atlas == nullptr or not loaded.atlas->valid())
	{
		m_diagnostics->writeOnce(key, "Atlas load failed: " + loaded.atlasPath);
		m_clips.emplace(key, PDAnimationClip());

		return nullptr;
	}

	m_diagnostics->write("Loaded clip " + key + " (" + std::to_string(loaded.frames.size()) + " frames)");

	return &m_clips.emplace(key, std::move(loaded)).first->second;
}
