#pragma once

#include <Engine/PDDiagnostics.h>
#include <Engine/PDMonitor.h>
#include <Engine/PDSettings.h>
#include <Engine/PDTexture.h>
#include <Pony/PDPonyCatalog.h>
#include <Pony/PDSceneEntry.h>
#include <UI/Windows/PDBehaviorEditor.h>

#include <imgui.h>

#include <string>
#include <unordered_map>
#include <vector>

class PDMainApplication;
class PDImGui;

struct PDSettingTarget
{
	std::string id;
	std::string label;
	std::string searchLabel;
	std::string previewPath;
};

struct PDCardInput
{
	bool click = false;
	bool doubleClick = false;
	bool rightClick = false;
};

class PDMainWindow
{
public:
	PDMainWindow(PDMainApplication& app, PDImGui& imgui, PDDiagnostics& diagnostics);

	void draw();
	void restoreScene();

	std::vector<PDSceneEntry> const& scene() const
	{
		return m_scene;
	}

private:
	static constexpr float NavWidth = 224.0f;
	static constexpr float SidebarPad = 12.0f;
	static constexpr float RunButtonHeight = 42.0f;
	static constexpr float QuitButtonHeight = 30.0f;
	static constexpr float FooterGap = 4.0f;
	static constexpr float CardWidth = 150.0f;
	static constexpr float ThumbHeight = 108.0f;
	static constexpr float CardPad = 8.0f;
	static constexpr float CardSpacing = 14.0f;
	static constexpr float RowPad = 10.0f;
	static constexpr float SceneRowHeight = 56.0f;
	static constexpr float SettingRowHeight = 48.0f;
	static constexpr int ThumbnailBudget = 24;

	enum class View
	{
		Browser,
		Scene,
		Features,
		Settings,
		Log,
		Behaviors
	};

	enum class VariantAction
	{
		Stage,
		Edit
	};

	void drawSidebar(float width, float height);
	void navItem(char const* label, View view);
	void drawRunControl();

	void drawContent();
	void drawBrowserView();
	void drawGroupCard(std::size_t idx);
	static float cardHeight();
	PDCardInput drawPonyCard(
		std::string const& name,
		std::string const& sub,
		PDTexture const* texture,
		bool highlight);
	PDTexture* thumbnail(std::string const& path);

	void openGroup(std::size_t idx, VariantAction action);
	void applyVariant(PDPonyGroup const& group, PDPonyPack const& variant);
	void openBehaviorEditor(std::string const& packPath, std::string const& displayName);

	void drawSceneView();
	bool drawSceneRow(PDSceneEntry* entry);
	void addToScene(std::string const& displayName, PDPonyPack const& pack);
	void removeSceneEntry(std::size_t idx);
	int sceneTotalQuantity() const;
	void drawForceBar();
	void forceCombo(char const* label, std::string* id);

	void drawLogView();
	void drawSettingsView();
	void drawMonitorRow(PDMonitor const& monitor);
	void exportConfig();
	void importConfig();

	void drawFeaturesView();
	void drawTargetButton();
	void drawTargetModal();
	bool drawTargetCard(int idx, std::vector<std::string> const& overrides);
	void drawVariantModal();

	static int gridColumns(float available, float* result);

	static void addFitImage(ImDrawList* drawList, PDTexture const* texture, ImVec2 areaMin, ImVec2 areaMax, float margin);
	static void addShadow(ImDrawList* drawList, ImVec2 rectMin, ImVec2 rectMax);

	PDMainApplication& m_app;
	PDImGui& m_imgui;
	PDDiagnostics& m_diagnostics;
	View m_activeView = View::Browser;

	PDPonyCatalog m_catalog;
	std::vector<std::string> m_searchNames;
	std::unordered_map<std::string, PDTexture> m_thumbnails;
	int m_thumbnailBudget = 0;

	char m_search[128] = "";
	int m_selectedGroup = -1;

	std::vector<PDSceneEntry> m_scene;
	bool m_sceneDirty = false;
	std::string m_forcePony;
	std::string m_forceTarget;

	std::string m_settingsTarget;
	std::string m_settingsTargetLabel;
	char m_targetFilter[64] = "";
	bool m_targetPickerOpen = false;
	bool m_featuresDirty = false;

	std::vector<PDSettingTarget> m_targets;

	int m_variantGroup = -1;
	VariantAction m_variantAction = VariantAction::Stage;
	bool m_variantOpen = false;

	PDBehaviorEditor m_behaviorEditor;
	View m_returnView = View::Browser;

	std::vector<PDMonitor> m_monitors;
	std::string m_configStatus;
	bool m_configFailed = false;
};
