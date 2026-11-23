#include <UI/Windows/PDBehaviorEditor.h>

#include <App/PDMainApplication.h>
#include <Core/PDPaths.h>
#include <Core/PDString.h>
#include <Engine/PDAnimationLoader.h>
#include <Engine/PDDiagnostics.h>
#include <Pony/PDPonyPackOverride.h>
#include <UI/PDImGui.h>
#include <UI/PDTheme.h>
#include <UI/PDWidgets.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

static nlohmann::json const& behaviorField(nlohmann::json const& behavior, char const* key)
{
	static nlohmann::json const missing;
	nlohmann::json::const_iterator const it = behavior.find(key);

	return it != behavior.end() ? *it : missing;
}

static std::string fieldString(nlohmann::json const& behavior, char const* key)
{
	nlohmann::json const& value = behaviorField(behavior, key);

	return value.is_string() ? value.get<std::string>() : std::string();
}

static double fieldNumber(nlohmann::json const& behavior, char const* key)
{
	nlohmann::json const& value = behaviorField(behavior, key);

	return value.is_number() ? value.get<double>() : 0.0;
}

static bool fieldBool(nlohmann::json const& behavior, char const* key)
{
	nlohmann::json const& value = behaviorField(behavior, key);

	return value.is_boolean() and value.get<bool>();
}

static int fieldInt(nlohmann::json const& behavior, char const* key)
{
	nlohmann::json const& value = behaviorField(behavior, key);

	if (not value.is_number_integer())
	{
		return 0;
	}

	double const number = value.get<double>();

	if (number < std::numeric_limits<int>::min() or number > std::numeric_limits<int>::max())
	{
		return 0;
	}

	return (int)number;
}

static double fieldDuration(nlohmann::json const& behavior, char const* key)
{
	nlohmann::json const& duration = behaviorField(behavior, "durationMs");

	if (not duration.is_object())
	{
		return 0.0;
	}

	return fieldNumber(duration, key);
}

static nlohmann::json const& documentBehaviors(nlohmann::json const& document)
{
	static nlohmann::json const empty = nlohmann::json::array();
	nlohmann::json::const_iterator const it = document.find("behaviors");

	return it != document.end() and it->is_array() ? *it : empty;
}

static bool readDocument(std::string const& path, nlohmann::json* result)
{
	assert(result != nullptr);

	std::ifstream stream(nativePath(path), std::ios::binary);

	if (not stream)
	{
		return false;
	}

	nlohmann::json document = nlohmann::json::parse(stream, nullptr, false);

	if (document.is_discarded() or not document.is_object())
	{
		return false;
	}

	nlohmann::json::const_iterator const pool = document.find("behaviors");

	if (pool != document.end())
	{
		if (not pool->is_array())
		{
			return false;
		}

		for (nlohmann::json const& entry : *pool)
		{
			if (not entry.is_object())
			{
				return false;
			}
		}
	}

	*result = std::move(document);

	return true;
}

static bool matchReference(nlohmann::json* object, char const* key, std::string const& id, bool apply)
{
	assert(object != nullptr);

	if (fieldString(*object, key) != id)
	{
		return false;
	}

	if (apply)
	{
		(*object)[key] = nullptr;
	}

	return true;
}

static bool matchListReference(nlohmann::json* list, std::string const& id, bool apply)
{
	assert(list != nullptr);

	nlohmann::json remaining = nlohmann::json::array();
	bool match = false;

	for (nlohmann::json const& value : *list)
	{
		if (value.is_string() and value.get<std::string>() == id)
		{
			match = true;
		}
		else
		{
			remaining.push_back(value);
		}
	}

	if (match and apply)
	{
		*list = std::move(remaining);
	}

	return match;
}

static bool combo(char const* id, int* current, std::vector<std::string> const& labels)
{
	std::vector<char const*> items;

	for (std::string const& label : labels)
	{
		items.push_back(label.c_str());
	}

	return ImGui::Combo(id, current, items.data(), (int)items.size());
}

PDBehaviorEditor::PDBehaviorEditor(PDMainApplication& app, PDImGui& imgui, PDDiagnostics& diagnostics)
	: m_app(app),
	  m_imgui(imgui),
	  m_diagnostics(diagnostics)
{
	m_movements = {
		"None",
		"MouseOver",
		"Sleep",
		"Dragged",
		"Horizontal_Only",
		"Vertical_Only",
		"Diagonal_Only",
		"Horizontal_Vertical",
		"Diagonal_Horizontal",
		"Diagonal_Vertical",
		"All"};
}

bool PDBehaviorEditor::open(std::string const& packPath, std::string const& displayName)
{
	nlohmann::json document;

	if (not readDocument(ponyPackDocumentPath(packPath), &document))
	{
		return false;
	}

	m_document = std::move(document);
	m_base = nlohmann::json();
	readDocument(pathString(nativePath(packPath) / "pony.json"), &m_base);

	m_previews.clear();
	m_packPath = packPath;
	m_displayName = displayName;
	m_hasOverride = ponyPackOverrideExists(packPath);
	m_expanded = -1;
	m_resetOpen = false;
	m_deleteOpen = false;
	m_deleteIdx = -1;
	m_deleteReferences.clear();
	m_search[0] = '\0';
	m_status.clear();
	m_mode = 0;

	rebuildIndex();

	return true;
}

void PDBehaviorEditor::close()
{
	m_packPath.clear();
	m_displayName.clear();
	m_document = nlohmann::json();
	m_modes.clear();
	m_linkTargets.clear();
	m_status.clear();
}

nlohmann::json const& PDBehaviorEditor::behaviors() const
{
	return documentBehaviors(m_document);
}

void PDBehaviorEditor::rebuildIndex()
{
	rebuildAnimations();
	rebuildBehaviors();
	rebuildModes();
}

void PDBehaviorEditor::rebuildAnimations()
{
	m_animations.clear();

	nlohmann::json::const_iterator const animations = m_document.find("animations");

	if (animations != m_document.end() and animations->is_object())
	{
		for (nlohmann::json::const_iterator it = animations->begin(); it != animations->end(); ++it)
		{
			if (not it.value().is_object())
			{
				continue;
			}

			if (fieldString(it.value(), "left").empty() and fieldString(it.value(), "right").empty())
			{
				continue;
			}

			m_animations.push_back(it.key());
		}
	}

	std::sort(m_animations.begin(), m_animations.end());
}

void PDBehaviorEditor::rebuildBehaviors()
{
	nlohmann::json const& base = documentBehaviors(m_base);

	m_linkTargets.clear();
	m_behaviorIds.clear();
	m_baseIdx.clear();

	for (nlohmann::json const& entry : behaviors())
	{
		std::string const link = fieldString(entry, "linkedBehavior");
		std::string const id = fieldString(entry, "id");
		std::size_t match = NoBaseIdx;

		if (not link.empty())
		{
			m_linkTargets.push_back(link);
		}

		for (std::size_t i = 0; i < base.size(); i += 1)
		{
			if (fieldString(base[i], "id") == id)
			{
				match = i;

				break;
			}
		}

		m_behaviorIds.push_back(id);
		m_baseIdx.push_back(match);
	}

	std::sort(m_linkTargets.begin(), m_linkTargets.end());
	m_linkTargets.erase(std::unique(m_linkTargets.begin(), m_linkTargets.end()), m_linkTargets.end());
}

void PDBehaviorEditor::rebuildModes()
{
	int const previousMode = m_mode;

	m_modes.clear();

	nlohmann::json::const_iterator const groups = m_document.find("behaviorGroups");

	if (groups != m_document.end() and groups->is_array())
	{
		for (nlohmann::json const& entry : *groups)
		{
			if (not entry.is_object())
			{
				continue;
			}

			int const id = fieldInt(entry, "id");

			if (id == 0 or modeName(id) != nullptr)
			{
				continue;
			}

			PDBehaviorMode mode;
			mode.id = id;
			mode.name = fieldString(entry, "name");
			m_modes.push_back(std::move(mode));
		}
	}

	for (nlohmann::json const& behavior : behaviors())
	{
		int const group = fieldInt(behavior, "group");

		if (group != 0 and modeName(group) == nullptr)
		{
			PDBehaviorMode mode;
			mode.id = group;
			m_modes.push_back(std::move(mode));
		}
	}

	m_mode = m_modes.empty() ? 0 : m_modes.front().id;

	for (PDBehaviorMode const& mode : m_modes)
	{
		if (mode.id == previousMode)
		{
			m_mode = previousMode;

			break;
		}
	}
}

nlohmann::json& PDBehaviorEditor::behavior(std::size_t idx)
{
	return m_document["behaviors"][idx];
}

nlohmann::json const* PDBehaviorEditor::baseValue(std::size_t idx, char const* key) const
{
	if (idx >= m_baseIdx.size())
	{
		return nullptr;
	}

	nlohmann::json const& base = documentBehaviors(m_base);
	std::size_t const baseIdx = m_baseIdx[idx];

	if (baseIdx >= base.size())
	{
		return nullptr;
	}

	nlohmann::json const& entry = base[baseIdx];
	nlohmann::json::const_iterator const it = entry.find(key);

	return it != entry.end() ? &(*it) : nullptr;
}

void PDBehaviorEditor::commit()
{
	std::string error;

	rebuildIndex();

	if (not writePonyPackOverride(m_packPath, m_document.dump(2), &error))
	{
		m_status = error;
		m_diagnostics.write("Behavior editor: " + error);

		return;
	}

	m_hasOverride = true;
	m_status.clear();
	m_app.reloadPack(m_packPath);
}

void PDBehaviorEditor::revertField(std::size_t idx, char const* key)
{
	nlohmann::json const* const base = baseValue(idx, key);

	if (base == nullptr)
	{
		behavior(idx).erase(key);
	}
	else
	{
		behavior(idx)[key] = *base;
	}

	commit();
}

void PDBehaviorEditor::resetToPack()
{
	std::string error;

	if (not removePonyPackOverride(m_packPath, &error))
	{
		m_status = error;
		m_diagnostics.write("Behavior editor: " + error);

		return;
	}

	m_hasOverride = false;
	m_status.clear();

	if (not m_base.is_null())
	{
		m_document = m_base;
	}

	rebuildIndex();
	m_app.reloadPack(m_packPath);
}

char const* PDBehaviorEditor::modeName(int id) const
{
	for (PDBehaviorMode const& mode : m_modes)
	{
		if (mode.id == id)
		{
			return mode.name.c_str();
		}
	}

	return nullptr;
}

bool PDBehaviorEditor::isLinkTarget(std::string const& id) const
{
	return std::find(m_linkTargets.begin(), m_linkTargets.end(), id) != m_linkTargets.end();
}

bool PDBehaviorEditor::eligibleInMode(std::size_t idx, int mode) const
{
	nlohmann::json const& behavior = behaviors()[idx];

	if (fieldNumber(behavior, "chance") <= 0.0 or fieldBool(behavior, "skip"))
	{
		return false;
	}

	int const group = fieldInt(behavior, "group");

	return group == 0 or group == mode;
}

double PDBehaviorEditor::eligibleTotal(int mode) const
{
	double total = 0.0;

	for (std::size_t i = 0; i < behaviors().size(); i += 1)
	{
		if (eligibleInMode(i, mode))
		{
			total += fieldNumber(behaviors()[i], "chance");
		}
	}

	return total;
}

PDBehaviorPreview const* PDBehaviorEditor::preview(std::string const& animation)
{
	std::unordered_map<std::string, PDBehaviorPreview>::const_iterator const it = m_previews.find(animation);

	if (it != m_previews.end())
	{
		return it->second.valid ? &it->second : nullptr;
	}

	if (m_previewBudget <= 0)
	{
		return nullptr;
	}

	m_previewBudget -= 1;

	PDBehaviorPreview& entry = m_previews[animation];
	std::string const relative = animationFile(animation);

	if (relative.empty())
	{
		return nullptr;
	}

	std::string const path = pathString((nativePath(m_packPath) / nativePath(relative)).lexically_normal());
	PDAnimationClip clip;
	std::string error;

	if (not loadAnimationClip(path, &clip, &error) or clip.frames.empty())
	{
		return nullptr;
	}

	PDAnimationFrame const& frame = clip.frames.front();

	entry.texture = PDTexture(m_imgui.device(), clip.atlasPath);
	entry.u0 = frame.u0;
	entry.v0 = frame.v0;
	entry.u1 = frame.u1;
	entry.v1 = frame.v1;
	entry.width = frame.width;
	entry.height = frame.height;
	entry.valid = entry.texture.valid();

	return entry.valid ? &entry : nullptr;
}

std::string PDBehaviorEditor::animationFile(std::string const& animation) const
{
	nlohmann::json::const_iterator const animations = m_document.find("animations");

	if (animations == m_document.end() or not animations->is_object())
	{
		return std::string();
	}

	nlohmann::json::const_iterator const it = animations->find(animation);

	if (it == animations->end() or not it->is_object())
	{
		return std::string();
	}

	std::string const right = fieldString(*it, "right");

	return right.empty() ? fieldString(*it, "left") : right;
}

nlohmann::json& PDBehaviorEditor::behaviorArray()
{
	if (not m_document["behaviors"].is_array())
	{
		m_document["behaviors"] = nlohmann::json::array();
	}

	return m_document["behaviors"];
}

std::string PDBehaviorEditor::uniqueId(std::string const& base) const
{
	std::string const stem = base.empty() ? std::string("behavior") : base;
	std::string candidate = stem;
	int suffix = 2;

	while (std::find(m_behaviorIds.begin(), m_behaviorIds.end(), candidate) != m_behaviorIds.end())
	{
		candidate = stem + " " + std::to_string(suffix);
		suffix += 1;
	}

	return candidate;
}

std::vector<std::string> PDBehaviorEditor::collectReferences(std::string const& id, bool apply)
{
	std::vector<std::string> refs;

	if (id.empty())
	{
		return refs;
	}

	for (nlohmann::json& entry : behaviorArray())
	{
		if (not entry.is_object())
		{
			continue;
		}

		std::string const owner = fieldString(entry, "id");

		if (matchReference(&entry, "linkedBehavior", id, apply))
		{
			refs.push_back("Play next on '" + owner + "'");
		}

		nlohmann::json::iterator const follow = entry.find("follow");

		if (follow == entry.end() or not follow->is_object())
		{
			continue;
		}

		if (matchReference(&*follow, "movingBehavior", id, apply))
		{
			refs.push_back("Follow moving behavior on '" + owner + "'");
		}

		if (matchReference(&*follow, "stoppedBehavior", id, apply))
		{
			refs.push_back("Follow stopped behavior on '" + owner + "'");
		}
	}

	nlohmann::json::iterator const effects = m_document.find("effects");

	if (effects != m_document.end() and effects->is_array())
	{
		for (nlohmann::json& effect : *effects)
		{
			if (effect.is_object() and matchReference(&effect, "behavior", id, apply))
			{
				refs.push_back("Effect '" + fieldString(effect, "id") + "'");
			}
		}
	}

	nlohmann::json::iterator const interactions = m_document.find("interactions");

	if (interactions == m_document.end() or not interactions->is_array())
	{
		return refs;
	}

	for (nlohmann::json& interaction : *interactions)
	{
		if (not interaction.is_object())
		{
			continue;
		}

		nlohmann::json::iterator const list = interaction.find("behaviors");

		if (list == interaction.end() or not list->is_array())
		{
			continue;
		}

		if (matchListReference(&*list, id, apply))
		{
			refs.push_back("Interaction '" + fieldString(interaction, "id") + "'");
		}
	}

	return refs;
}

void PDBehaviorEditor::addBehavior()
{
	nlohmann::json& pool = behaviorArray();
	nlohmann::json entry = pool.empty() ? nlohmann::json::object() : pool.front();

	entry["id"] = uniqueId("new-behavior");
	entry["name"] = "New behavior";
	entry["chance"] = 0.1;
	entry["durationMs"] = nlohmann::json{{"min", 5000}, {"max", 15000}};
	entry["movement"] = "None";
	entry["speedPxPerSec"] = 0.0;
	entry["group"] = 0;
	entry["skip"] = false;
	entry["preventAnimationLoop"] = false;
	entry["linkedBehavior"] = nullptr;

	for (char const* key : {"follow", "target", "startSpeech", "endSpeech"})
	{
		if (entry.contains(key))
		{
			entry[key] = nullptr;
		}
	}

	if (entry.contains("special"))
	{
		entry["special"] = false;
	}

	if (not m_animations.empty())
	{
		entry["animation"] = m_animations.front();
	}

	pool.push_back(std::move(entry));
	m_expanded = (int)pool.size() - 1;
	commit();
}

void PDBehaviorEditor::duplicateBehavior(std::size_t idx)
{
	nlohmann::json& pool = behaviorArray();

	if (idx >= pool.size())
	{
		return;
	}

	nlohmann::json entry = pool[idx];
	entry["id"] = uniqueId(fieldString(pool[idx], "id"));
	entry["name"] = fieldString(pool[idx], "name") + " copy";

	pool.push_back(std::move(entry));
	m_expanded = (int)pool.size() - 1;
	commit();
}

void PDBehaviorEditor::requestDelete(std::size_t idx)
{
	if (idx >= behaviors().size())
	{
		return;
	}

	m_deleteIdx = (int)idx;
	m_deleteReferences = collectReferences(fieldString(behaviors()[idx], "id"), false);
	m_deleteOpen = true;
}

void PDBehaviorEditor::drawDeleteModal()
{
	if (m_deleteOpen)
	{
		m_deleteOpen = false;
		ImGui::OpenPopup("Delete behavior");
	}

	if (not beginModal("Delete behavior", ImVec2(520.0f, 360.0f)))
	{
		return;
	}

	if (m_deleteIdx < 0 or m_deleteIdx >= (int)behaviors().size())
	{
		ImGui::CloseCurrentPopup();
		endModal();

		return;
	}

	std::size_t const idx = (std::size_t)m_deleteIdx;
	std::string const id = fieldString(behaviors()[idx], "id");

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
	ImGui::TextWrapped("Delete '%s' from %s?", id.c_str(), m_displayName.c_str());
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 10.0f));
	ImGui::BeginChild("deleteBody", ImVec2(0.0f, -46.0f));

	if (m_deleteReferences.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextFaint);
		ImGui::TextUnformatted("Nothing else references it.");
		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::AccentText);
		ImGui::Text("%zu references will be cleared:", m_deleteReferences.size());
		ImGui::PopStyleColor();
		ImGui::Dummy(ImVec2(0.0f, 6.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);

		for (std::string const& reference : m_deleteReferences)
		{
			ImGui::BulletText("%s", reference.c_str());
		}

		ImGui::PopStyleColor();
	}

	ImGui::EndChild();

	if (ImGui::Button("Delete", ImVec2(110.0f, 32.0f)))
	{
		collectReferences(id, true);
		behaviorArray().erase(idx);
		m_expanded = -1;
		m_deleteIdx = -1;
		commit();
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel", ImVec2(110.0f, 32.0f)))
	{
		ImGui::CloseCurrentPopup();
	}

	endModal();
}

void PDBehaviorEditor::drawFieldLabel(char const* label)
{
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine(LabelWidth);
	ImGui::SetNextItemWidth(-(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x));
}

void PDBehaviorEditor::drawRevertMarker(std::size_t idx, char const* key)
{
	nlohmann::json const* const base = baseValue(idx, key);
	nlohmann::json const& current = behaviorField(behaviors()[idx], key);
	bool const present = not current.is_null();
	bool const differs = base == nullptr ? present : (not present or *base != current);
	float const size = ImGui::GetFrameHeight();

	ImGui::SameLine();

	if (not differs)
	{
		ImGui::Dummy(ImVec2(size, size));

		return;
	}

	if (ImGui::Button("x", ImVec2(size, size)))
	{
		revertField(idx, key);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Reset to the pack value");
	}
}

void PDBehaviorEditor::drawNameField(std::size_t idx)
{
	ImGui::PushID("name");
	drawFieldLabel("Name");

	std::string value = fieldString(behaviors()[idx], "name");

	if (ImGui::InputText("##v", &value))
	{
		behavior(idx)["name"] = value;
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		commit();
	}

	drawRevertMarker(idx, "name");
	ImGui::PopID();
}

void PDBehaviorEditor::drawChanceField(std::size_t idx)
{
	ImGui::PushID("chance");
	drawFieldLabel("Chance");

	double value = fieldNumber(behaviors()[idx], "chance");

	if (ImGui::InputDouble("##v", &value, 0.0, 0.0, "%.6g"))
	{
		behavior(idx)["chance"] = value < 0.0 ? 0.0 : value;
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		commit();
	}

	drawRevertMarker(idx, "chance");
	ImGui::PopID();
}

void PDBehaviorEditor::drawSpeedField(std::size_t idx)
{
	ImGui::PushID("speed");
	drawFieldLabel("Speed (px/s)");

	double value = fieldNumber(behaviors()[idx], "speedPxPerSec");

	if (ImGui::InputDouble("##v", &value, 0.0, 0.0, "%.6g"))
	{
		behavior(idx)["speedPxPerSec"] = value < 0.0 ? 0.0 : value;
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		commit();
	}

	drawRevertMarker(idx, "speedPxPerSec");
	ImGui::PopID();
}

void PDBehaviorEditor::drawDurationField(std::size_t idx)
{
	ImGui::PushID("durationMs");
	drawFieldLabel("Duration (ms)");

	int values[2] =
	{
		(int)std::clamp(fieldDuration(behaviors()[idx], "min"), 0.0, (double)std::numeric_limits<int>::max()),
		(int)std::clamp(fieldDuration(behaviors()[idx], "max"), 0.0, (double)std::numeric_limits<int>::max())
	};

	if (ImGui::InputInt2("##v", values))
	{
		values[0] = values[0] < 0 ? 0 : values[0];
		values[1] = values[1] < values[0] ? values[0] : values[1];

		behavior(idx)["durationMs"] = nlohmann::json{{"min", values[0]}, {"max", values[1]}};
	}

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		commit();
	}

	drawRevertMarker(idx, "durationMs");
	ImGui::PopID();
}

void PDBehaviorEditor::drawGroupField(std::size_t idx)
{
	ImGui::PushID("group");
	drawFieldLabel("Mode");

	std::vector<std::string> labels;
	int current = 0;
	int const group = fieldInt(behaviors()[idx], "group");

	labels.push_back("0  -  any mode");

	for (std::size_t i = 0; i < m_modes.size(); i += 1)
	{
		char label[96];
		std::snprintf(
			label,
			sizeof(label),
			"%d  -  %s",
			m_modes[i].id,
			m_modes[i].name.empty() ? "unnamed" : m_modes[i].name.c_str());
		labels.push_back(label);

		if (m_modes[i].id == group)
		{
			current = (int)i + 1;
		}
	}

	if (combo("##v", &current, labels))
	{
		behavior(idx)["group"] = current == 0 ? 0 : m_modes[(std::size_t)(current - 1)].id;
		commit();
	}

	drawRevertMarker(idx, "group");
	ImGui::PopID();
}

void PDBehaviorEditor::drawFlagField(std::size_t idx, char const* key, char const* label)
{
	ImGui::PushID(key);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine(LabelWidth);

	bool value = fieldBool(behaviors()[idx], key);

	if (ImGui::Checkbox("##v", &value))
	{
		behavior(idx)[key] = value;
		commit();
	}

	drawRevertMarker(idx, key);
	ImGui::PopID();
}

void PDBehaviorEditor::drawChoiceField(
	std::size_t idx,
	char const* key,
	char const* label,
	std::vector<std::string> const& options,
	bool allowNone)
{
	ImGui::PushID(key);
	drawFieldLabel(label);

	std::string const value = fieldString(behaviors()[idx], key);
	std::vector<std::string> labels;
	int current = 0;

	if (allowNone)
	{
		labels.push_back("(none)");
	}

	for (std::string const& option : options)
	{
		if (option == value)
		{
			current = (int)labels.size();
		}

		labels.push_back(option);
	}

	if (not value.empty() and std::find(options.begin(), options.end(), value) == options.end())
	{
		current = (int)labels.size();
		labels.push_back(value);
	}

	if (combo("##v", &current, labels))
	{
		if (allowNone and current == 0)
		{
			behavior(idx).erase(key);
		}
		else
		{
			behavior(idx)[key] = labels[(std::size_t)current];
		}

		commit();
	}

	drawRevertMarker(idx, key);
	ImGui::PopID();
}

void PDBehaviorEditor::drawFields(std::size_t idx)
{
	drawNameField(idx);
	drawChoiceField(idx, "animation", "Animation", m_animations, false);
	drawChanceField(idx);
	drawDurationField(idx);
	drawChoiceField(idx, "movement", "Movement", m_movements, false);
	drawSpeedField(idx);
	drawChoiceField(idx, "linkedBehavior", "Play next", m_behaviorIds, true);
	drawGroupField(idx);

	ImGui::Dummy(ImVec2(0.0f, 6.0f));

	drawFlagField(idx, "skip", "Never pick at random");
	drawFlagField(idx, "special", "Extended behavior");
	drawFlagField(idx, "preventAnimationLoop", "Play animation once");
}

void PDBehaviorEditor::drawRowPreview(ImDrawList* drawList, ImVec2 thumbMin, std::string const& animation)
{
	PDBehaviorPreview const* const frame = preview(animation);

	if (frame == nullptr)
	{
		return;
	}

	if (frame->width <= 0.0f or frame->height <= 0.0f)
	{
		return;
	}

	float scale = std::min(PreviewSize / frame->width, PreviewSize / frame->height);
	scale = std::min(scale, 1.0f);

	float const drawWidth = frame->width * scale;
	float const drawHeight = frame->height * scale;
	ImVec2 const imageMin(
		thumbMin.x + (PreviewSize - drawWidth) * 0.5f,
		thumbMin.y + (PreviewSize - drawHeight) * 0.5f);

	drawList->AddImage(
		reinterpret_cast<ImTextureID>(frame->texture.view()),
		imageMin,
		ImVec2(imageMin.x + drawWidth, imageMin.y + drawHeight),
		ImVec2(frame->u0, frame->v0),
		ImVec2(frame->u1, frame->v1));
}

std::string PDBehaviorEditor::rowChip(std::size_t idx, double total, ImVec4* result) const
{
	assert(result != nullptr);

	nlohmann::json const& behavior = behaviors()[idx];
	int const group = fieldInt(behavior, "group");

	if (eligibleInMode(idx, m_mode))
	{
		double const share = total > 0.0 ? fieldNumber(behavior, "chance") / total * 100.0 : 0.0;
		char chip[64];
		std::snprintf(chip, sizeof(chip), "%.1f %%", share);
		*result = PDTheme::White;

		return chip;
	}

	if (group != 0 and group != m_mode)
	{
		*result = PDTheme::TextFaint;

		return "other mode";
	}

	if (isLinkTarget(fieldString(behavior, "id")))
	{
		*result = PDTheme::TextDim;

		return "link only";
	}

	*result = PDTheme::Stop;

	return "unreachable";
}

void PDBehaviorEditor::drawBehaviorRow(std::size_t idx, double total)
{
	nlohmann::json const& behavior = behaviors()[idx];
	std::string const id = fieldString(behavior, "id");
	std::string const name = fieldString(behavior, "name");
	int const group = fieldInt(behavior, "group");

	PDRowCard const card = beginRowCard(ListRowHeight, m_expanded == (int)idx);

	float const lineHeight = ImGui::GetTextLineHeight();
	float const firstY = card.position.y + (ListRowHeight - lineHeight * 2.0f - 2.0f) * 0.5f;
	float const secondY = firstY + lineHeight + 2.0f;
	float const textX = card.position.x + 10.0f + PreviewSize + 12.0f;

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 const thumbMin(card.position.x + 10.0f, card.position.y + (ListRowHeight - PreviewSize) * 0.5f);
	ImVec2 const thumbMax(thumbMin.x + PreviewSize, thumbMin.y + PreviewSize);
	drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(PDTheme::ThumbBg), 0.0f);

	if (ImGui::IsRectVisible(card.position, card.rectMax))
	{
		drawRowPreview(drawList, thumbMin, fieldString(behavior, "animation"));
	}

	drawList->AddText(
		ImVec2(textX, firstY),
		ImGui::GetColorU32(PDTheme::White),
		name.empty() ? id.c_str() : name.c_str());

	char subtitle[192];
	std::snprintf(
		subtitle,
		sizeof(subtitle),
		"chance %.6g  -  %.1f-%.1f s  -  %s",
		fieldNumber(behavior, "chance"),
		fieldDuration(behavior, "min") / 1000.0,
		fieldDuration(behavior, "max") / 1000.0,
		fieldString(behavior, "movement").c_str());

	drawList->AddText(ImVec2(textX, secondY), ImGui::GetColorU32(PDTheme::TextFaint), subtitle);

	ImVec4 chipColor;
	std::string const chip = rowChip(idx, total, &chipColor);
	float const chipWidth = ImGui::CalcTextSize(chip.c_str()).x;
	float badgeRight = card.rectMax.x - 10.0f;

	drawList->AddText(
		ImVec2(card.rectMax.x - 10.0f - chipWidth, firstY),
		ImGui::GetColorU32(chipColor),
		chip.c_str());

	if (group != 0)
	{
		char const* const label = modeName(group);
		char const* const badge = label != nullptr and *label != '\0' ? label : "grouped";
		float const badgeWidth = ImGui::CalcTextSize(badge).x;

		drawList->AddText(
			ImVec2(badgeRight - badgeWidth, secondY),
			ImGui::GetColorU32(PDTheme::AccentText),
			badge);

		badgeRight -= badgeWidth + 10.0f;
	}

	if (fieldBool(behavior, "special"))
	{
		float const extWidth = ImGui::CalcTextSize("EXT").x;
		drawList->AddText(
			ImVec2(badgeRight - extWidth, secondY),
			ImGui::GetColorU32(PDTheme::TextDim),
			"EXT");
	}

	if (card.clicked)
	{
		m_expanded = m_expanded == (int)idx ? -1 : (int)idx;
	}

	endRowCard(card, ListRowHeight, 4.0f);
}

void PDBehaviorEditor::drawDetail()
{
	if (m_expanded < 0 or m_expanded >= (int)behaviors().size())
	{
		return;
	}

	std::size_t const idx = (std::size_t)m_expanded;
	std::string const id = fieldString(behaviors()[idx], "id");
	std::string const name = fieldString(behaviors()[idx], "name");

	char title[256];
	std::snprintf(
		title,
		sizeof(title),
		"%s  -  %s###behaviorDetail",
		m_displayName.c_str(),
		name.empty() ? id.c_str() : name.c_str());

	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 0.0f), ImVec2(900.0f, 4000.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

	if (ImGui::Begin(title, &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextFaint);
		ImGui::TextUnformatted(id.c_str());
		ImGui::PopStyleColor();
		ImGui::Dummy(ImVec2(0.0f, 8.0f));

		ImGui::PushID(m_packPath.c_str());
		ImGui::PushID((int)idx);
		drawFields(idx);
		ImGui::PopID();
		ImGui::PopID();

		ImGui::Dummy(ImVec2(0.0f, 12.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 8.0f));

		if (ImGui::Button("Duplicate", ImVec2(110.0f, 30.0f)))
		{
			duplicateBehavior(idx);
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, PDTheme::Stop);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PDTheme::StopHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, PDTheme::StopPress);

		if (ImGui::Button("Delete", ImVec2(110.0f, 30.0f)))
		{
			requestDelete(idx);
		}

		ImGui::PopStyleColor(3);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	if (not open)
	{
		m_expanded = -1;
	}
}

void PDBehaviorEditor::drawResetModal()
{
	if (m_resetOpen)
	{
		m_resetOpen = false;
		ImGui::OpenPopup("Reset behaviors");
	}

	if (not beginModal("Reset behaviors", ImVec2(420.0f, 180.0f)))
	{
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
	ImGui::TextWrapped("Discard every edit for %s and go back to the values the pack ships with?", m_displayName.c_str());
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 16.0f));

	if (ImGui::Button("Reset", ImVec2(110.0f, 32.0f)))
	{
		resetToPack();
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel", ImVec2(110.0f, 32.0f)))
	{
		ImGui::CloseCurrentPopup();
	}

	endModal();
}

void PDBehaviorEditor::drawModeCombo()
{
	std::vector<std::string> labels;
	int current = 0;

	for (std::size_t i = 0; i < m_modes.size(); i += 1)
	{
		if (m_modes[i].name.empty())
		{
			labels.push_back("Mode " + std::to_string(m_modes[i].id));
		}
		else
		{
			labels.push_back(m_modes[i].name);
		}

		if (m_modes[i].id == m_mode)
		{
			current = (int)i;
		}
	}

	ImGui::SetNextItemWidth(150.0f);

	if (combo("##mode", &current, labels))
	{
		m_mode = m_modes[(std::size_t)current].id;
	}
}

bool PDBehaviorEditor::drawToolbar()
{
	beginToolbar();

	bool const back = ImGui::Button("Back");

	if (m_modes.size() > 1)
	{
		ImGui::SameLine();
		drawModeCombo();
	}

	ImGui::SameLine();

	if (ImGui::Button("Add"))
	{
		addBehavior();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Add a new behavior to this pack");
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(not m_hasOverride);

	if (ImGui::Button("Reset"))
	{
		m_resetOpen = true;
	}

	ImGui::EndDisabled();

	if (m_hasOverride and ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Discard every edit for this pack");
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	ImGui::InputTextWithHint("##behaviorSearch", "Search behaviors", m_search, sizeof(m_search));

	char summary[160];
	std::snprintf(
		summary,
		sizeof(summary),
		"%s  -  %zu behaviors%s",
		m_displayName.c_str(),
		behaviors().size(),
		m_hasOverride ? "  -  overridden" : "");
	toolbarSummary(summary);

	endToolbar();

	return back;
}

void PDBehaviorEditor::drawBehaviorList()
{
	std::string const filter = toLower(m_search);
	double const total = eligibleTotal(m_mode);

	m_previewBudget = 6;

	ImGui::BeginChild("behaviorList");

	for (std::size_t i = 0; i < behaviors().size(); i += 1)
	{
		nlohmann::json const& behavior = behaviors()[i];

		if (not filter.empty())
		{
			std::string const haystack = toLower(fieldString(behavior, "id") + " " + fieldString(behavior, "name"));

			if (haystack.find(filter) == std::string::npos)
			{
				continue;
			}
		}

		ImGui::PushID((int)i);
		drawBehaviorRow(i, total);
		ImGui::PopID();
	}

	ImGui::EndChild();
}

bool PDBehaviorEditor::draw()
{
	bool const back = drawToolbar();

	drawResetModal();
	drawDeleteModal();

	if (back)
	{
		return false;
	}

	if (not m_status.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::Stop);
		ImGui::TextWrapped("%s", m_status.c_str());
		ImGui::PopStyleColor();
		ImGui::Dummy(ImVec2(0.0f, 6.0f));
	}

	drawBehaviorList();

	return true;
}
