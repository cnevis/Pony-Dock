#pragma once

#include <Engine/Components/PDAnimation.h>
#include <Engine/Components/PDBehavior.h>
#include <Engine/Components/PDInteraction.h>
#include <Engine/Components/PDLabel.h>
#include <Engine/Components/PDPack.h>
#include <Engine/Components/PDPosition.h>
#include <Engine/Components/PDSprite.h>
#include <Engine/PDAnimationCache.h>
#include <Engine/PDDiagnostics.h>
#include <Engine/PDSpriteRenderer.h>
#include <Engine/PDTextCache.h>
#include <Engine/PDTextureCache.h>
#include <Library/PDPonyPackData.h>
#include <Library/PDSettingsStore.h>
#include <Math/PDRect.h>

#include <d3d11.h>

#include <entt/entt.hpp>

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class PDScene
{
public:
	void initialize(ID3D11Device* device, PDDiagnostics* diagnostics, PDSettingsStore* settings);
	void spawnEntity(std::string const& packPath, float x, float y);
	void clear();
	void reshuffle();
	void reloadPack(std::string const& packPath);

	void updateMouse(int x, int y, bool pressed, bool released);
	bool wantsMouse() const;

	void tick(float deltaSeconds, int boundsWidth, int boundsHeight, std::vector<PDRect> const& monitors);
	void draw(PDSpriteRenderer& renderer) const;

private:
	PDPonyPackData const* pack(std::string const& packPath);
	PDSettings const& settings(entt::entity entity) const;
	std::size_t pickBehavior(entt::entity entity);
	std::size_t pickHeldBehavior(entt::entity entity);
	void enterBehavior(entt::entity entity, std::size_t index, int depth);
	void advanceBehavior(entt::entity entity);
	void updateBehavior(entt::entity entity, float deltaSeconds);
	void updateInteractions(float deltaSeconds);
	bool startInteraction(entt::entity entity, PDPonyInteractionData const& interaction);
	void placeEntity(entt::entity entity);
	void moveEntity(entt::entity entity, float deltaSeconds);
	void constrain(entt::entity entity, float* vx, float* vy);
	int monitorAt(float x, float y) const;

	void advanceAnimations(float deltaSeconds);
	void writeSpriteFromAnimation(PDAnimation const& animation, PDSprite* sprite) const;
	bool playAnimation(entt::entity entity, std::string const& name, bool loop, bool restart);
	void setFacing(entt::entity entity, bool facingRight);

	PDDiagnostics* m_diagnostics = nullptr;
	PDSettingsStore* m_settings = nullptr;
	PDSettingsSnapshot m_snapshot;
	PDTextureCache m_textures;
	PDTextCache m_labels;
	PDAnimationCache m_animations;
	std::unordered_map<std::string, PDPonyPackData> m_packs;
	entt::registry m_registry;
	std::vector<PDRect> m_monitors;
	std::mt19937 m_rng{std::random_device{}()};
	std::uniform_real_distribution<float> m_random{0.0f, 1.0f};

	entt::entity m_hovered = entt::null;
	entt::entity m_dragged = entt::null;
	float m_dragOffsetX = 0.0f;
	float m_dragOffsetY = 0.0f;
};
