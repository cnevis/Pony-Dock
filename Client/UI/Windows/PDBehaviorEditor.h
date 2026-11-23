#pragma once

#include <Engine/PDTexture.h>

#include <nlohmann/json.hpp>

#include <imgui.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class PDDiagnostics;
class PDImGui;
class PDMainApplication;

struct PDBehaviorMode
{
	int id = 0;
	std::string name;
};

struct PDBehaviorPreview
{
	PDTexture texture;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
	float width = 0.0f;
	float height = 0.0f;
	bool valid = false;
};

class PDBehaviorEditor
{
public:
	PDBehaviorEditor(PDMainApplication& app, PDImGui& imgui, PDDiagnostics& diagnostics);

	bool open(std::string const& packPath, std::string const& displayName);
	void close();

	bool draw();
	void drawDetail();

private:
	static constexpr float ListRowHeight = 46.0f;
	static constexpr float PreviewSize = 34.0f;
	static constexpr float LabelWidth = 150.0f;
	static constexpr std::size_t NoBaseIdx = (std::size_t)(-1);

	void rebuildIndex();
	void rebuildAnimations();
	void rebuildBehaviors();
	void rebuildModes();

	bool drawToolbar();
	void drawModeCombo();
	void drawBehaviorList();
	void drawBehaviorRow(std::size_t idx, double total);
	void drawRowPreview(ImDrawList* drawList, ImVec2 thumbMin, std::string const& animation);
	std::string rowChip(std::size_t idx, double total, ImVec4* result) const;
	void drawFields(std::size_t idx);
	void drawResetModal();
	void drawDeleteModal();

	void addBehavior();
	void duplicateBehavior(std::size_t idx);
	void requestDelete(std::size_t idx);

	std::vector<std::string> collectReferences(std::string const& id, bool apply);
	std::string uniqueId(std::string const& base) const;
	nlohmann::json& behaviorArray();
	PDBehaviorPreview const* preview(std::string const& animation);
	std::string animationFile(std::string const& animation) const;

	void drawFieldLabel(char const* label);
	void drawRevertMarker(std::size_t idx, char const* key);

	void drawNameField(std::size_t idx);
	void drawChanceField(std::size_t idx);
	void drawSpeedField(std::size_t idx);
	void drawDurationField(std::size_t idx);
	void drawGroupField(std::size_t idx);
	void drawFlagField(std::size_t idx, char const* key, char const* label);
	void drawChoiceField(
		std::size_t idx,
		char const* key,
		char const* label,
		std::vector<std::string> const& options,
		bool allowNone);

	nlohmann::json& behavior(std::size_t idx);
	nlohmann::json const& behaviors() const;
	nlohmann::json const* baseValue(std::size_t idx, char const* key) const;

	bool eligibleInMode(std::size_t idx, int mode) const;
	double eligibleTotal(int mode) const;
	char const* modeName(int id) const;
	bool isLinkTarget(std::string const& id) const;

	void commit();
	void revertField(std::size_t idx, char const* key);
	void resetToPack();

	PDMainApplication& m_app;
	PDImGui& m_imgui;
	PDDiagnostics& m_diagnostics;

	std::unordered_map<std::string, PDBehaviorPreview> m_previews;
	int m_previewBudget = 0;

	std::string m_packPath;
	std::string m_displayName;
	std::string m_status;

	nlohmann::json m_document;
	nlohmann::json m_base;

	std::vector<PDBehaviorMode> m_modes;
	std::vector<std::string> m_linkTargets;
	std::vector<std::string> m_behaviorIds;
	std::vector<std::string> m_animations;
	std::vector<std::string> m_movements;
	std::vector<std::size_t> m_baseIdx;

	std::vector<std::string> m_deleteReferences;

	int m_mode = 0;
	int m_expanded = -1;
	int m_deleteIdx = -1;
	bool m_hasOverride = false;
	bool m_resetOpen = false;
	bool m_deleteOpen = false;
	char m_search[96] = "";
};
