#include <Engine/PDScene.h>

#include <Core/PDPaths.h>
#include <Library/PDPonyPackLoader.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>
#include <string_view>

enum class PDMovement
{
	None,
	Horizontal,
	Vertical,
	Diagonal,
	HorizontalVertical,
	DiagonalHorizontal,
	DiagonalVertical,
	All
};

static std::pair<std::string_view, PDMovement> const g_Movements[] =
{
	{"Horizontal_Only", PDMovement::Horizontal},
	{"Vertical_Only", PDMovement::Vertical},
	{"Diagonal_Only", PDMovement::Diagonal},
	{"Horizontal_Vertical", PDMovement::HorizontalVertical},
	{"Diagonal_Horizontal", PDMovement::DiagonalHorizontal},
	{"Diagonal_Vertical", PDMovement::DiagonalVertical},
	{"All", PDMovement::All}
};

static std::string_view const g_HeldMovements[] =
{
	"Dragged", "None", "MouseOver", "Sleep"
};

static PDMovement movement(std::string const& name)
{
	for (std::pair<std::string_view, PDMovement> const& entry : g_Movements)
	{
		if (entry.first == name)
		{
			return entry.second;
		}
	}

	return PDMovement::None;
}

static std::size_t findBehavior(PDPonyPackData const& data, std::string const& id)
{
	for (std::size_t i = 0; i < data.behaviors.size(); i += 1)
	{
		if (data.behaviors[i].id == id)
		{
			return i;
		}
	}

	return data.behaviors.size();
}

static bool eligible(PDPonyBehaviorData const& behavior, PDBehavior const& state, PDSettings const& settings)
{
	if (behavior.skip or (behavior.special and not settings.extended))
	{
		return false;
	}

	return behavior.group == 0 or behavior.group == state.group;
}

void PDScene::initialize(ID3D11Device* device, PDDiagnostics* diagnostics, PDSettingsStore* settings)
{
	assert(device != nullptr);
	assert(diagnostics != nullptr);
	assert(settings != nullptr);

	m_diagnostics = diagnostics;
	m_settings = settings;
	m_settings->refresh(&m_snapshot);
	m_textures.initialize(device);
	m_labels.initialize(device);
	m_animations.initialize(m_textures, *diagnostics);
}

PDSettings const& PDScene::settings(entt::entity entity) const
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	std::unordered_map<std::string, PDSettings>::const_iterator const found = m_snapshot.packs.find(data.id);

	return found != m_snapshot.packs.end() ? found->second : m_snapshot.defaults;
}

void PDScene::reloadPack(std::string const& packPath)
{
	std::string const key = pathString(nativePath(packPath).lexically_normal());
	std::unordered_map<std::string, PDPonyPackData>::iterator const existing = m_packs.find(key);

	if (existing == m_packs.end())
	{
		return;
	}

	PDPonyPackData loaded;
	std::string error;

	if (not loadPonyPack(key, loaded, error))
	{
		m_diagnostics->write("Pack reload failed: " + error);

		return;
	}

	existing->second = std::move(loaded);

	for (entt::entity const entity : m_registry.view<PDPack, PDBehavior, PDInteraction>())
	{
		if (m_registry.get<PDPack>(entity).data != &existing->second)
		{
			continue;
		}

		PDBehavior& state = m_registry.get<PDBehavior>(entity);
		PDBehavior reset;

		reset.placed = state.placed;
		reset.monitor = state.monitor;
		reset.group = existing->second.groups.empty() ? 0 : existing->second.groups.front().id;
		state = reset;
		m_registry.get<PDInteraction>(entity).busy = false;
		m_registry.get<PDInteraction>(entity).cooldowns.clear();
	}

	m_diagnostics->write("Reloaded pack " + existing->second.id);
}

void PDScene::updateMouse(int x, int y, bool pressed, bool released)
{
	float const pointX = (float)x;
	float const pointY = (float)y;

	m_settings->refresh(&m_snapshot);

	if (released or (m_registry.valid(m_dragged) and not settings(m_dragged).dragging))
	{
		m_dragged = entt::null;
	}

	m_hovered = entt::null;

	for (entt::entity const entity : m_registry.view<PDPosition const, PDSprite const>())
	{
		PDPosition const& position = m_registry.get<PDPosition>(entity);
		PDSprite const& sprite = m_registry.get<PDSprite>(entity);
		float const left = position.x - sprite.offsetX;
		float const top = position.y - sprite.offsetY;

		if (pointX < left or pointX > left + sprite.width)
		{
			continue;
		}

		if (pointY < top or pointY > top + sprite.height)
		{
			continue;
		}

		m_hovered = entity;
	}

	if (pressed and not released and m_registry.valid(m_hovered) and settings(m_hovered).dragging)
	{
		PDPosition const& position = m_registry.get<PDPosition>(m_hovered);

		m_dragged = m_hovered;
		m_dragOffsetX = pointX - position.x;
		m_dragOffsetY = pointY - position.y;
	}

	if (not m_registry.valid(m_dragged))
	{
		return;
	}

	PDPosition& position = m_registry.get<PDPosition>(m_dragged);

	position.x = pointX - m_dragOffsetX;
	position.y = pointY - m_dragOffsetY;
}

bool PDScene::wantsMouse() const
{
	return m_registry.valid(m_dragged)
		or (m_registry.valid(m_hovered) and settings(m_hovered).dragging);
}

PDPonyPackData const* PDScene::pack(std::string const& packPath)
{
	std::string const key = pathString(nativePath(packPath).lexically_normal());
	std::unordered_map<std::string, PDPonyPackData>::const_iterator const existing = m_packs.find(key);

	if (existing != m_packs.end())
	{
		return existing->second.valid ? &existing->second : nullptr;
	}

	PDPonyPackData loaded;
	std::string error;

	if (not loadPonyPack(key, loaded, error))
	{
		m_diagnostics->writeOnce(key, "Pack load failed: " + error);
		m_packs.emplace(key, PDPonyPackData());

		return nullptr;
	}

	m_diagnostics->write("Loaded pack " + loaded.id + " (" + std::to_string(loaded.animations.size()) + " animations, " + std::to_string(loaded.behaviors.size()) + " behaviors)");

	return &m_packs.emplace(key, std::move(loaded)).first->second;
}

void PDScene::spawnEntity(std::string const& packPath, float x, float y)
{
	PDPonyPackData const* const data = pack(packPath);

	if (data == nullptr)
	{
		return;
	}

	PDTexture const* const preview = m_textures.texture(data->previewPath);
	entt::entity const entity = m_registry.create();

	m_registry.emplace<PDPosition>(entity, x, y);
	m_registry.emplace<PDSprite>(entity, preview, 96.0f, 96.0f);
	m_registry.emplace<PDPack>(entity, data);
	m_registry.emplace<PDAnimation>(entity);
	m_registry.emplace<PDLabel>(entity);
	m_registry.emplace<PDInteraction>(entity);

	PDBehavior& state = m_registry.emplace<PDBehavior>(entity);

	state.group = data->groups.empty() ? 0 : data->groups.front().id;
	enterBehavior(entity, pickBehavior(entity), 0);
}

void PDScene::clear()
{
	m_hovered = entt::null;
	m_dragged = entt::null;
	m_registry.clear();
}

void PDScene::reshuffle()
{
	for (entt::entity const entity : m_registry.view<PDBehavior, PDInteraction>())
	{
		PDBehavior& state = m_registry.get<PDBehavior>(entity);

		m_registry.get<PDInteraction>(entity).busy = false;
		state.active = false;
		state.grabbed = false;
		state.linkDepth = 0;
	}
}

std::size_t PDScene::pickBehavior(entt::entity entity)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDBehavior const& state = m_registry.get<PDBehavior>(entity);
	PDSettings const& enabled = settings(entity);
	double total = 0.0;
	std::size_t fallback = data.behaviors.size();

	for (std::size_t i = 0; i < data.behaviors.size(); i += 1)
	{
		PDPonyBehaviorData const& behavior = data.behaviors[i];

		if (not eligible(behavior, state, enabled))
		{
			continue;
		}

		if (fallback == data.behaviors.size())
		{
			fallback = i;
		}

		if (behavior.chance > 0.0f)
		{
			total += behavior.chance;
		}
	}

	if (total <= 0.0)
	{
		return fallback;
	}

	double roll = m_random(m_rng) * total;

	for (std::size_t i = 0; i < data.behaviors.size(); i += 1)
	{
		PDPonyBehaviorData const& behavior = data.behaviors[i];

		if (behavior.chance <= 0.0f or not eligible(behavior, state, enabled))
		{
			continue;
		}

		fallback = i;
		roll -= behavior.chance;

		if (roll <= 0.0)
		{
			return i;
		}
	}

	return fallback;
}

std::size_t PDScene::pickHeldBehavior(entt::entity entity)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDSettings const& enabled = settings(entity);
	std::vector<std::size_t> matches;

	for (std::string_view const name : g_HeldMovements)
	{
		for (std::size_t i = 0; i < data.behaviors.size(); i += 1)
		{
			PDPonyBehaviorData const& behavior = data.behaviors[i];

			if (behavior.movement == name and (enabled.extended or not behavior.special))
			{
				matches.push_back(i);
			}
		}

		if (not matches.empty())
		{
			return matches[std::uniform_int_distribution<std::size_t>(0, matches.size() - 1)(m_rng)];
		}
	}

	return data.behaviors.size();
}

void PDScene::enterBehavior(entt::entity entity, std::size_t index, int depth)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDBehavior& state = m_registry.get<PDBehavior>(entity);
	bool horizontal = false;
	bool vertical = false;

	state.active = index < data.behaviors.size();
	state.vx = 0.0f;
	state.vy = 0.0f;

	if (not state.active)
	{
		m_registry.get<PDAnimation>(entity).finished = true;

		return;
	}

	PDPonyBehaviorData const& behavior = data.behaviors[index];

	if (behavior.special and not settings(entity).extended)
	{
		state.active = false;
		m_registry.get<PDAnimation>(entity).finished = true;

		return;
	}

	state.index = index;
	state.linkDepth = depth;
	state.timer = std::max(0.25f, behavior.durationMinSeconds + m_random(m_rng) *
		(std::max(behavior.durationMinSeconds, behavior.durationMaxSeconds) - behavior.durationMinSeconds));

	if (behavior.group != 0)
	{
		state.group = behavior.group;
	}

	switch (movement(behavior.movement))
	{
		case PDMovement::Horizontal:
		{
			horizontal = true;

			break;
		}

		case PDMovement::Vertical:
		{
			vertical = true;

			break;
		}

		case PDMovement::Diagonal:
		{
			horizontal = true;
			vertical = true;

			break;
		}

		case PDMovement::HorizontalVertical:
		{
			horizontal = m_random(m_rng) < 0.5f;
			vertical = not horizontal;

			break;
		}

		case PDMovement::DiagonalHorizontal:
		{
			horizontal = true;
			vertical = m_random(m_rng) < 0.5f;

			break;
		}

		case PDMovement::DiagonalVertical:
		{
			horizontal = m_random(m_rng) < 0.5f;
			vertical = true;

			break;
		}

		case PDMovement::All:
		{
			horizontal = m_random(m_rng) < 0.5f;
			vertical = m_random(m_rng) < 0.5f;
			horizontal = horizontal or not vertical;

			break;
		}

		case PDMovement::None:
		{
			break;
		}
	}

	float const signX = m_random(m_rng) < 0.5f ? -1.0f : 1.0f;
	float const signY = m_random(m_rng) < 0.5f ? -1.0f : 1.0f;
	float const speed = std::max(0.0f, behavior.speedPxPerSec);

	if (horizontal and vertical)
	{
		float const angle = (30.0f + m_random(m_rng) * 45.0f) * std::numbers::pi_v<float> / 180.0f;

		state.vx = std::cos(angle) * signX * speed;
		state.vy = std::sin(angle) * signY * speed;
	}
	else if (vertical)
	{
		state.vy = signY * speed;
	}
	else if (horizontal)
	{
		state.vx = signX * speed;
	}

	if (state.vx != 0.0f)
	{
		setFacing(entity, state.vx > 0.0f);
	}

	playAnimation(entity, behavior.animation, not behavior.preventAnimationLoop, behavior.preventAnimationLoop);
}

void PDScene::advanceBehavior(entt::entity entity)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDBehavior const& state = m_registry.get<PDBehavior>(entity);
	PDSettings const& enabled = settings(entity);

	m_registry.get<PDInteraction>(entity).busy = false;

	if (state.active and enabled.linked and state.linkDepth < 16)
	{
		std::string const& linked = data.behaviors[state.index].linkedBehavior;

		if (not linked.empty())
		{
			std::size_t const next = findBehavior(data, linked);

			if (next < data.behaviors.size() and (enabled.extended or not data.behaviors[next].special))
			{
				enterBehavior(entity, next, state.linkDepth + 1);

				return;
			}
		}
	}

	enterBehavior(entity, pickBehavior(entity), 0);
}

void PDScene::updateBehavior(entt::entity entity, float deltaSeconds)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDBehavior& state = m_registry.get<PDBehavior>(entity);
	PDSettings const& enabled = settings(entity);

	if (state.active and data.behaviors[state.index].special and not enabled.extended)
	{
		m_registry.get<PDInteraction>(entity).busy = false;
		state.active = false;
		state.grabbed = false;
	}

	if (not state.active)
	{
		enterBehavior(entity, pickBehavior(entity), 0);
	}

	if (not state.placed)
	{
		placeEntity(entity);
	}

	if (entity == m_dragged)
	{
		if (not state.grabbed)
		{
			std::size_t const held = pickHeldBehavior(entity);

			m_registry.get<PDInteraction>(entity).busy = false;
			state.grabbed = true;

			if (held < data.behaviors.size())
			{
				enterBehavior(entity, held, 0);
			}
		}

		return;
	}

	if (state.grabbed)
	{
		PDPosition const& position = m_registry.get<PDPosition>(entity);
		PDSprite const& sprite = m_registry.get<PDSprite>(entity);

		state.grabbed = false;
		state.monitor = monitorAt(position.x - sprite.offsetX + sprite.width * 0.5f,
			position.y - sprite.offsetY + sprite.height * 0.5f);
		advanceBehavior(entity);

		return;
	}

	if (not state.active)
	{
		return;
	}

	PDAnimation const& animation = m_registry.get<PDAnimation>(entity);

	state.timer -= deltaSeconds;

	if (state.timer <= 0.0f or (data.behaviors[state.index].preventAnimationLoop and animation.finished))
	{
		advanceBehavior(entity);

		return;
	}

	moveEntity(entity, deltaSeconds);
}

int PDScene::monitorAt(float x, float y) const
{
	for (std::size_t i = 0; i < m_monitors.size(); i += 1)
	{
		PDRect const& area = m_monitors[i];

		if (x >= area.x and x < area.x + area.width and y >= area.y and y < area.y + area.height)
		{
			return (int)i;
		}
	}

	return -1;
}

void PDScene::placeEntity(entt::entity entity)
{
	if (m_monitors.empty())
	{
		return;
	}

	PDBehavior& state = m_registry.get<PDBehavior>(entity);
	PDPosition& position = m_registry.get<PDPosition>(entity);
	PDSprite const& sprite = m_registry.get<PDSprite>(entity);
	PDSettings const& enabled = settings(entity);
	std::size_t const i = std::uniform_int_distribution<std::size_t>(0, m_monitors.size() - 1)(m_rng);
	PDRect const& area = m_monitors[i];
	float const width = std::max(0.0f, area.width - sprite.width);
	float const height = std::max(0.0f, area.height - sprite.height);

	state.monitor = (int)i;
	state.placed = true;
	position.x = area.x + sprite.offsetX + m_random(m_rng) * width;

	switch (enabled.start)
	{
		case PDSpawnPosition::Anywhere:
		{
			position.y = area.y + sprite.offsetY + m_random(m_rng) * height;

			break;
		}

		case PDSpawnPosition::Ground:
		{
			position.y = area.y + sprite.offsetY + height;

			break;
		}

		case PDSpawnPosition::Center:
		{
			position.x = area.x + sprite.offsetX + (area.width - sprite.width) * 0.5f;
			position.y = area.y + sprite.offsetY + (area.height - sprite.height) * 0.5f;

			break;
		}
	}
}

void PDScene::constrain(entt::entity entity, float* vx, float* vy)
{
	assert(vx != nullptr);
	assert(vy != nullptr);

	if (m_monitors.empty())
	{
		return;
	}

	PDBehavior& state = m_registry.get<PDBehavior>(entity);
	PDPosition& position = m_registry.get<PDPosition>(entity);
	PDSprite const& sprite = m_registry.get<PDSprite>(entity);
	float centerX = position.x - sprite.offsetX + sprite.width * 0.5f;
	float centerY = position.y - sprite.offsetY + sprite.height * 0.5f;
	int const current = monitorAt(centerX, centerY);

	if (current >= 0)
	{
		state.monitor = current;
	}
	else if (state.monitor < 0 or (std::size_t)state.monitor >= m_monitors.size())
	{
		float nearest = std::numeric_limits<float>::max();

		for (std::size_t i = 0; i < m_monitors.size(); i += 1)
		{
			PDRect const& area = m_monitors[i];
			float const x = std::clamp(centerX, area.x, area.x + area.width);
			float const y = std::clamp(centerY, area.y, area.y + area.height);
			float const distance = (centerX - x) * (centerX - x) + (centerY - y) * (centerY - y);

			if (distance < nearest)
			{
				nearest = distance;
				state.monitor = (int)i;
			}
		}
	}

	PDRect area = m_monitors[(std::size_t)state.monitor];
	float const left = position.x - sprite.offsetX;

	if (sprite.width >= area.width)
	{
		position.x = area.x + (area.width - sprite.width) * 0.5f + sprite.offsetX;
		*vx = 0.0f;
	}
	else if (left < area.x and monitorAt(area.x - 0.5f, centerY) < 0)
	{
		position.x = area.x + sprite.offsetX;
		*vx = std::abs(*vx);
	}
	else if (left + sprite.width > area.x + area.width and monitorAt(area.x + area.width + 0.5f, centerY) < 0)
	{
		position.x = area.x + area.width - sprite.width + sprite.offsetX;
		*vx = -std::abs(*vx);
	}

	centerX = position.x - sprite.offsetX + sprite.width * 0.5f;

	int const crossed = monitorAt(centerX, centerY);

	if (crossed >= 0)
	{
		state.monitor = crossed;
		area = m_monitors[(std::size_t)crossed];
	}

	float const top = position.y - sprite.offsetY;

	if (sprite.height >= area.height)
	{
		position.y = area.y + (area.height - sprite.height) * 0.5f + sprite.offsetY;
		*vy = 0.0f;
	}
	else if (top < area.y and monitorAt(centerX, area.y - 0.5f) < 0)
	{
		position.y = area.y + sprite.offsetY;
		*vy = std::abs(*vy);
	}
	else if (top + sprite.height > area.y + area.height and monitorAt(centerX, area.y + area.height + 0.5f) < 0)
	{
		position.y = area.y + area.height - sprite.height + sprite.offsetY;
		*vy = -std::abs(*vy);
	}

	centerY = position.y - sprite.offsetY + sprite.height * 0.5f;

	int const final = monitorAt(centerX, centerY);

	if (final >= 0)
	{
		state.monitor = final;
	}
}

void PDScene::moveEntity(entt::entity entity, float deltaSeconds)
{
	PDSettings const& enabled = settings(entity);

	if (not enabled.movement)
	{
		return;
	}

	PDBehavior& state = m_registry.get<PDBehavior>(entity);
	PDPosition& position = m_registry.get<PDPosition>(entity);
	PDAnimation const& animation = m_registry.get<PDAnimation>(entity);
	float vx = state.vx;
	float vy = state.vy;

	if (not enabled.flying and vy != 0.0f)
	{
		float const sign = vx != 0.0f ? vx : (animation.facingRight ? 1.0f : -1.0f);

		vx = std::copysign(std::hypot(vx, vy), sign);
		vy = 0.0f;
	}

	position.x += vx * enabled.speed * deltaSeconds;
	position.y += vy * enabled.speed * deltaSeconds;
	constrain(entity, &vx, &vy);

	if (vx != 0.0f)
	{
		state.vx = std::copysign(std::abs(state.vx), vx);
		setFacing(entity, vx > 0.0f);
	}

	if (vy != 0.0f)
	{
		state.vy = std::copysign(std::abs(state.vy), vy);
	}
}

bool PDScene::startInteraction(entt::entity entity, PDPonyInteractionData const& interaction)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDPosition const& position = m_registry.get<PDPosition>(entity);
	PDSprite const& sprite = m_registry.get<PDSprite>(entity);
	PDSettings const& enabled = settings(entity);
	float const x = position.x - sprite.offsetX + sprite.width * 0.5f;
	float const y = position.y - sprite.offsetY + sprite.height * 0.5f;

	if (interaction.proximityPx <= 0.0f)
	{
		return false;
	}

	for (entt::entity const other : m_registry.view<PDPack, PDPosition, PDSprite, PDBehavior, PDInteraction>())
	{
		if (other == entity or other == m_dragged or m_registry.get<PDInteraction>(other).busy)
		{
			continue;
		}

		PDSettings const& otherSettings = settings(other);

		if (not otherSettings.interactions)
		{
			continue;
		}

		PDPonyPackData const& otherData = *m_registry.get<PDPack>(other).data;

		if (std::find(interaction.targets.begin(), interaction.targets.end(), otherData.id) == interaction.targets.end())
		{
			continue;
		}

		PDPosition const& otherPosition = m_registry.get<PDPosition>(other);
		PDSprite const& otherSprite = m_registry.get<PDSprite>(other);
		float const dx = otherPosition.x - otherSprite.offsetX + otherSprite.width * 0.5f - x;
		float const dy = otherPosition.y - otherSprite.offsetY + otherSprite.height * 0.5f - y;

		if (std::hypot(dx, dy) > interaction.proximityPx)
		{
			continue;
		}

		std::vector<std::pair<std::size_t, std::size_t>> matches;

		for (std::string const& id : interaction.behaviors)
		{
			std::size_t const a = findBehavior(data, id);
			std::size_t const b = findBehavior(otherData, id);

			if (a == data.behaviors.size() or b == otherData.behaviors.size())
			{
				continue;
			}

			if ((data.behaviors[a].special and not enabled.extended)
				or (otherData.behaviors[b].special and not otherSettings.extended))
			{
				continue;
			}

			matches.emplace_back(a, b);
		}

		if (matches.empty())
		{
			continue;
		}

		std::size_t const i = std::uniform_int_distribution<std::size_t>(0, matches.size() - 1)(m_rng);

		enterBehavior(entity, matches[i].first, 0);
		enterBehavior(other, matches[i].second, 0);
		m_registry.get<PDInteraction>(entity).busy = true;
		m_registry.get<PDInteraction>(other).busy = true;

		return true;
	}

	return false;
}

void PDScene::updateInteractions(float deltaSeconds)
{
	for (entt::entity const entity : m_registry.view<PDInteraction>())
	{
		PDInteraction& state = m_registry.get<PDInteraction>(entity);

		for (std::pair<std::string const, float>& entry : state.cooldowns)
		{
			entry.second = std::max(0.0f, entry.second - deltaSeconds);
		}

		state.countdown -= deltaSeconds;

		if (not settings(entity).interactions or entity == m_dragged)
		{
			state.busy = false;
		}
	}

	for (entt::entity const entity : m_registry.view<PDInteraction>())
	{
		PDInteraction& state = m_registry.get<PDInteraction>(entity);

		if (not settings(entity).interactions or entity == m_dragged)
		{
			continue;
		}

		if (state.busy or state.countdown > 0.0f)
		{
			continue;
		}

		PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;

		state.countdown = 0.25f;

		for (PDPonyInteractionData const& interaction : data.interactions)
		{
			if (interaction.activation != "One" or state.cooldowns[interaction.id] > 0.0f)
			{
				continue;
			}

			if (m_random(m_rng) >= interaction.chance)
			{
				continue;
			}

			if (startInteraction(entity, interaction))
			{
				state.cooldowns[interaction.id] = std::max(3.0f, interaction.reactivationDelaySeconds);

				break;
			}
		}
	}
}

bool PDScene::playAnimation(entt::entity entity, std::string const& name, bool loop, bool restart)
{
	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	PDAnimation& animation = m_registry.get<PDAnimation>(entity);
	std::unordered_map<std::string, PDPonyAnimationRef>::const_iterator const found = data.animations.find(name);

	if (found == data.animations.end())
	{
		m_diagnostics->writeOnce(data.id + "/" + name, "Unknown animation '" + name + "' in pack " + data.id);

		return false;
	}

	std::string const& path = animation.facingRight ? found->second.right : found->second.left;
	PDAnimationClip const* const clip = m_animations.clip(path);

	if (clip == nullptr)
	{
		return false;
	}

	bool const unchanged = animation.clip == clip and animation.name == name and animation.loop == loop;

	if (unchanged and not restart and not animation.finished)
	{
		return true;
	}

	animation.clip = clip;
	animation.name = name;
	animation.loop = loop;
	animation.finished = false;
	animation.frame = 0;
	animation.elapsedSeconds = 0.0f;
	writeSpriteFromAnimation(animation, &m_registry.get<PDSprite>(entity));

	return true;
}

void PDScene::setFacing(entt::entity entity, bool facingRight)
{
	PDAnimation& animation = m_registry.get<PDAnimation>(entity);

	if (animation.facingRight == facingRight)
	{
		return;
	}

	PDPonyPackData const& data = *m_registry.get<PDPack>(entity).data;
	std::unordered_map<std::string, PDPonyAnimationRef>::const_iterator const found = data.animations.find(animation.name);

	animation.facingRight = facingRight;

	if (found == data.animations.end())
	{
		return;
	}

	PDAnimationClip const* const clip = m_animations.clip(facingRight ? found->second.right : found->second.left);

	if (clip == nullptr or clip == animation.clip)
	{
		return;
	}

	animation.clip = clip;
	animation.frame = std::min(animation.frame, (int)clip->frames.size() - 1);
	animation.elapsedSeconds = std::min(animation.elapsedSeconds, clip->frames[(std::size_t)animation.frame].durationSeconds);
	writeSpriteFromAnimation(animation, &m_registry.get<PDSprite>(entity));
}

void PDScene::writeSpriteFromAnimation(PDAnimation const& animation, PDSprite* sprite) const
{
	assert(sprite != nullptr);

	if (animation.clip == nullptr or animation.clip->frames.empty())
	{
		return;
	}

	PDAnimationFrame const& frame = animation.clip->frames[(std::size_t)animation.frame];

	sprite->texture = animation.clip->atlas;
	sprite->width = frame.width;
	sprite->height = frame.height;
	sprite->u0 = frame.u0;
	sprite->v0 = frame.v0;
	sprite->u1 = frame.u1;
	sprite->v1 = frame.v1;
	sprite->offsetX = animation.clip->pivotX;
	sprite->offsetY = animation.clip->pivotY;
}

void PDScene::advanceAnimations(float deltaSeconds)
{
	for (entt::entity const entity : m_registry.view<PDAnimation, PDSprite>())
	{
		PDAnimation& animation = m_registry.get<PDAnimation>(entity);
		PDSprite& sprite = m_registry.get<PDSprite>(entity);

		if (animation.clip == nullptr or animation.clip->frames.empty())
		{
			continue;
		}

		std::vector<PDAnimationFrame> const& frames = animation.clip->frames;

		if (not animation.finished)
		{
			int guard = (int)frames.size() + 1;

			animation.elapsedSeconds += deltaSeconds;

			while (guard > 0 and animation.elapsedSeconds >= frames[(std::size_t)animation.frame].durationSeconds)
			{
				animation.elapsedSeconds -= frames[(std::size_t)animation.frame].durationSeconds;
				guard -= 1;

				if (animation.frame + 1 < (int)frames.size())
				{
					animation.frame += 1;
				}
				else if (animation.loop)
				{
					animation.frame = 0;
				}
				else
				{
					animation.finished = true;
					animation.elapsedSeconds = 0.0f;

					break;
				}
			}
		}

		writeSpriteFromAnimation(animation, &sprite);
	}
}

void PDScene::tick(float deltaSeconds, int boundsWidth, int boundsHeight, std::vector<PDRect> const& monitors)
{
	m_settings->refresh(&m_snapshot);
	m_monitors.clear();

	for (PDRect const& area : monitors)
	{
		if (area.width > 0.0f and area.height > 0.0f)
		{
			m_monitors.push_back(area);
		}
	}

	if (m_monitors.empty() and boundsWidth > 0 and boundsHeight > 0)
	{
		m_monitors.push_back(PDRect{0.0f, 0.0f, (float)boundsWidth, (float)boundsHeight});
	}

	if (m_registry.valid(m_dragged) and not settings(m_dragged).dragging)
	{
		m_dragged = entt::null;
	}

	advanceAnimations(deltaSeconds);

	for (entt::entity const entity : m_registry.view<PDBehavior>())
	{
		updateBehavior(entity, deltaSeconds);
	}

	updateInteractions(deltaSeconds);

	for (entt::entity const entity : m_registry.view<PDLabel, PDPack>())
	{
		PDLabel& label = m_registry.get<PDLabel>(entity);
		std::string const text = entity == m_hovered and settings(entity).names
			? m_registry.get<PDPack>(entity).data->name : std::string();

		if (text != label.text)
		{
			label.text = text;
			label.texture = text.empty() ? nullptr : m_labels.texture(text);
		}
	}
}

void PDScene::draw(PDSpriteRenderer& renderer) const
{
	for (entt::entity const entity : m_registry.view<PDPosition const, PDSprite const>())
	{
		PDPosition const& position = m_registry.get<PDPosition>(entity);
		PDSprite const& sprite = m_registry.get<PDSprite>(entity);

		if (sprite.texture == nullptr or not sprite.texture->valid())
		{
			continue;
		}

		renderer.draw(
			sprite.texture->view(),
			std::floor(position.x - sprite.offsetX + 0.5f),
			std::floor(position.y - sprite.offsetY + 0.5f),
			sprite.width,
			sprite.height,
			sprite.u0,
			sprite.v0,
			sprite.u1,
			sprite.v1);
	}

	for (entt::entity const entity : m_registry.view<PDPosition const, PDSprite const, PDLabel const>())
	{
		PDLabel const& label = m_registry.get<PDLabel>(entity);

		if (label.texture == nullptr or not label.texture->valid())
		{
			continue;
		}

		PDPosition const& position = m_registry.get<PDPosition>(entity);
		PDSprite const& sprite = m_registry.get<PDSprite>(entity);
		float const width = (float)label.texture->width();
		float const height = (float)label.texture->height();

		renderer.draw(
			label.texture->view(),
			std::floor(position.x - sprite.offsetX + (sprite.width - width) * 0.5f + 0.5f),
			std::floor(position.y - sprite.offsetY - height - 4.0f + 0.5f),
			width,
			height,
			0.0f,
			0.0f,
			1.0f,
			1.0f);
	}
}
