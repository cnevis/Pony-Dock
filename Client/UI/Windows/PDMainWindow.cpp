#include <UI/Windows/PDMainWindow.h>

#include <App/PDMainApplication.h>
#include <Core/PDPaths.h>
#include <Core/PDString.h>
#include <UI/PDFileDialog.h>
#include <UI/PDImGui.h>
#include <UI/PDTheme.h>
#include <UI/PDWidgets.h>

#include <windows.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <utility>

void PDMainWindow::addImageFitted(ImDrawList *drawList, PDTexture const *texture, ImVec2 areaMin, ImVec2 areaMax, float margin)
{
	if (texture == nullptr or not texture->valid() or texture->width() <= 0 or texture->height() <= 0)
	{
		return;
	}

	const float areaWidth = areaMax.x - areaMin.x - margin * 2.0f;
	const float areaHeight = areaMax.y - areaMin.y - margin * 2.0f;

	if (areaWidth <= 0.0f or areaHeight <= 0.0f)
	{
		return;
	}

	const float textureWidth = static_cast<float>(texture->width());
	const float textureHeight = static_cast<float>(texture->height());
	float scale = std::min(areaWidth / textureWidth, areaHeight / textureHeight);
	scale = std::min(scale, 2.0f);

	const float drawWidth = textureWidth * scale;
	const float drawHeight = textureHeight * scale;
	const float offsetX = areaMin.x + margin + (areaWidth - drawWidth) * 0.5f;
	const float offsetY = areaMin.y + margin + (areaHeight - drawHeight) * 0.5f;

	drawList->AddImage(
		reinterpret_cast<ImTextureID>(texture->view()),
		ImVec2(offsetX, offsetY),
		ImVec2(offsetX + drawWidth, offsetY + drawHeight));
}

void PDMainWindow::addShadow(ImDrawList *drawList, ImVec2 rectMin, ImVec2 rectMax)
{
	for (int index = 0; index < 6; index += 1)
	{
		const float spread = ShadowSpread * static_cast<float>(index + 1) / 6.0f;
		const float fade = 1.0f - static_cast<float>(index) / 6.0f;
		const ImVec4 color(PDTheme::Shadow.x, PDTheme::Shadow.y, PDTheme::Shadow.z, PDTheme::Shadow.w * fade);

		drawList->AddRect(
			ImVec2(rectMin.x - spread, rectMin.y - spread),
			ImVec2(rectMax.x + spread, rectMax.y + spread),
			ImGui::GetColorU32(color),
			0.0f,
			0,
			1.0f);
	}
}

PDMainWindow::PDMainWindow(PDMainApplication &app, PDImGui &host, PDDiagnostics &diagnostics)
	: m_app(app),
	  m_host(host),
	  m_diagnostics(diagnostics),
	  m_behaviorEditor(app, host, diagnostics)
{
	m_catalog.load(packsRoot());

	for (PDPonyGroup const &group : m_catalog.groups())
	{
		for (PDPonyPack const &variant : group.variants)
		{
			PDSettingTarget target;
			target.id = variant.id;
			target.label = group.displayName;
			target.previewPath = variant.previewPath;

			if (group.variants.size() > 1)
			{
				target.label += "  (" + variant.id + ")";
			}

			m_targets.push_back(std::move(target));
		}
	}
}

void PDMainWindow::draw()
{
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin(
		"Pony Dock",
		nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

	ImGui::PopStyleVar(3);

	const float height = ImGui::GetContentRegionAvail().y;
	drawSidebar(NavWidth, height);
	ImGui::SameLine(0.0f, 0.0f);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 18.0f));
	ImGui::BeginChild("content", ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding);
	drawContent();
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	ImDrawList *drawList = ImGui::GetWindowDrawList();
	const ImVec2 windowPos = ImGui::GetWindowPos();
	drawList->AddLine(
		ImVec2(windowPos.x + NavWidth, windowPos.y),
		ImVec2(windowPos.x + NavWidth, windowPos.y + height),
		ImGui::GetColorU32(PDTheme::Divider),
		1.0f);

	ImGui::End();

	if (m_activeView == View::Behaviors)
	{
		m_behaviorEditor.drawDetail();
	}

	if (m_featuresDirty and (m_activeView != View::Features or not ImGui::IsAnyItemActive()))
	{
		m_featuresDirty = false;

		if (not m_app.settings().save())
		{
			m_diagnostics.write("Could not save feature settings");
		}
	}
}

void PDMainWindow::drawSidebar(float width, float height)
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, PDTheme::Sidebar);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SidebarPad, SidebarPad));
	ImGui::BeginChild("sidebar", ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
	ImGui::PopStyleVar();

	navItem("Browser", View::Browser);
	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	navItem("Scene", View::Scene);
	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	navItem("Features", View::Features);
	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	navItem("Settings", View::Settings);
	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	navItem("Log", View::Log);

	const float spacing = ImGui::GetStyle().ItemSpacing.y;
	const float footerHeight = RunButtonHeight + FooterGap + QuitButtonHeight + spacing * 3.0f;
	const float remaining = ImGui::GetContentRegionAvail().y - footerHeight;

	if (remaining > 0.0f)
	{
		ImGui::Dummy(ImVec2(0.0f, remaining));
	}

	drawRunControl();

	ImGui::Dummy(ImVec2(0.0f, FooterGap));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextFaint);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PDTheme::SidebarHover);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, PDTheme::SidebarHover);

	if (ImGui::Button("Quit Pony Dock", ImVec2(ImGui::GetContentRegionAvail().x, QuitButtonHeight)))
	{
		m_app.requestExit();
	}

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void PDMainWindow::navItem(const char *label, View view)
{
	constexpr float itemHeight = 38.0f;
	const ImVec2 position = ImGui::GetCursorScreenPos();
	const ImVec2 size(ImGui::GetContentRegionAvail().x, itemHeight);

	ImGui::PushID(static_cast<int>(view));
	const bool pressed = ImGui::InvisibleButton("nav", size);
	const bool hovered = ImGui::IsItemHovered();
	ImGui::PopID();

	const bool active = m_activeView == view;
	ImDrawList *drawList = ImGui::GetWindowDrawList();
	const ImVec2 rectMax(position.x + size.x, position.y + size.y);

	if (active)
	{
		drawList->AddRectFilled(position, rectMax, ImGui::GetColorU32(PDTheme::AccentSoft), 0.0f);
		drawList->AddRectFilled(position, ImVec2(position.x + 3.0f, rectMax.y), ImGui::GetColorU32(PDTheme::Accent), 0.0f);
	}
	else if (hovered)
	{
		drawList->AddRectFilled(position, rectMax, ImGui::GetColorU32(PDTheme::SidebarHover), 0.0f);
	}

	const ImVec4 &textColor = active ? PDTheme::AccentText : (hovered ? PDTheme::White : PDTheme::TextDim);
	const float textY = position.y + (itemHeight - ImGui::GetTextLineHeight()) * 0.5f;
	drawList->AddText(ImVec2(position.x + 16.0f, textY), ImGui::GetColorU32(textColor), label);

	if (view == View::Scene)
	{
		const int total = sceneTotalQuantity();

		if (total > 0)
		{
			char badge[16];
			std::snprintf(badge, sizeof(badge), "%d", total);

			const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
			constexpr float badgePadX = 8.0f;
			const ImVec2 badgeMax(rectMax.x - 10.0f, position.y + (itemHeight + badgeSize.y + 6.0f) * 0.5f);
			const ImVec2 badgeMin(badgeMax.x - badgeSize.x - badgePadX * 2.0f, position.y + (itemHeight - badgeSize.y - 6.0f) * 0.5f);

			drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(active ? PDTheme::Accent : PDTheme::Badge), 0.0f);
			drawList->AddText(ImVec2(badgeMin.x + badgePadX, badgeMin.y + 3.0f), ImGui::GetColorU32(active ? PDTheme::White : PDTheme::TextDim), badge);
		}
	}

	if (pressed)
	{
		setView(view);
	}
}

void PDMainWindow::drawRunControl()
{
	const bool running = m_app.isRunning();
	const float width = ImGui::GetContentRegionAvail().x;

	if (running)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, PDTheme::Stop);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PDTheme::StopHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, PDTheme::StopPress);

		if (ImGui::Button("Stop", ImVec2(width, RunButtonHeight)))
		{
			m_app.stopScene();
		}
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, PDTheme::Start);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PDTheme::StartHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, PDTheme::StartPress);

		ImGui::BeginDisabled(m_scene.empty());

		if (ImGui::Button("Start", ImVec2(width, RunButtonHeight)))
		{
			m_app.startScene(m_scene);
		}

		ImGui::EndDisabled();

		if (m_scene.empty() and ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Add ponies from Browser first");
		}
	}

	ImGui::PopStyleColor(3);
}

void PDMainWindow::setView(View view)
{
	m_activeView = view;
}

void PDMainWindow::drawContent()
{
	switch (m_activeView)
	{
		case View::Browser:
		{
			drawBrowserView();

			break;
		}

		case View::Scene:
		{
			drawSceneView();

			break;
		}

		case View::Features:
		{
			drawFeaturesView();

			break;
		}

		case View::Settings:
		{
			drawSettingsView();

			break;
		}

		case View::Behaviors:
		{
			if (not m_behaviorEditor.draw())
			{
				m_behaviorEditor.close();
				m_activeView = m_returnView;
			}

			break;
		}

		case View::Log:
		{
			drawLogView();

			break;
		}

		default:
		{
			break;
		}
	}
}

void PDMainWindow::drawBrowserView()
{
	m_thumbnailBudget = 24;

	beginToolbar();

	ImGui::SetNextItemWidth(250.0f);
	ImGui::InputTextWithHint("##search", "Search ponies", m_search, sizeof(m_search));

	endToolbar();

	ImGui::BeginChild("grid");

	std::string const query = toLower(m_search);
	std::vector<PDPonyGroup> const &groups = m_catalog.groups();

	const float available = ImGui::GetContentRegionAvail().x;
	const int columns = std::max(1, static_cast<int>((available + CardSpacing) / (CardWidth + CardSpacing)));

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(CardSpacing, CardSpacing));

	int shown = 0;

	for (std::size_t index = 0; index < groups.size(); index += 1)
	{
		PDPonyGroup const &group = groups[index];

		if (not query.empty() and toLower(group.displayName).find(query) == std::string::npos)
		{
			continue;
		}

		if (shown % columns != 0)
		{
			ImGui::SameLine();
		}

		shown += 1;

		std::string sub;

		if (group.variants.size() > 1)
		{
			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%zu variants", group.variants.size());
			sub = buffer;
		}

		ImGui::PushID(static_cast<int>(index));

		PDCardInput const input = drawPonyCard(
			group.displayName,
			sub,
			thumbnail(group.variants.front().previewPath),
			m_selectedGroup == static_cast<int>(index));

		if (input.clicked)
		{
			m_selectedGroup = static_cast<int>(index);
		}

		if (input.doubleClicked)
		{
			openGroup(index, VariantStage);
		}

		if (input.rightClicked)
		{
			m_selectedGroup = static_cast<int>(index);
			openGroup(index, VariantEdit);
		}

		ImGui::PopID();
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	drawVariantModal();
}

void PDMainWindow::openGroup(std::size_t index, int action)
{
	PDPonyGroup const &group = m_catalog.groups()[index];
	m_variantAction = action;

	if (group.variants.size() > 1)
	{
		m_variantGroup = static_cast<int>(index);
		m_variantOpen = true;

		return;
	}

	applyVariant(group, group.variants.front());
}

void PDMainWindow::applyVariant(PDPonyGroup const &group, PDPonyPack const &variant)
{
	if (m_variantAction == VariantEdit)
	{
		openBehaviorEditor(variant.packPath, group.displayName);

		return;
	}

	addToScene(group.displayName, variant);
}

void PDMainWindow::openBehaviorEditor(std::string const &packPath, std::string const &displayName)
{
	if (not m_behaviorEditor.open(packPath, displayName))
	{
		m_diagnostics.write("Cannot open the behavior editor for " + packPath);

		return;
	}

	m_returnView = m_activeView;
	m_activeView = View::Behaviors;
}

PDCardInput PDMainWindow::drawPonyCard(
	std::string const &name,
	std::string const &sub,
	PDTexture const *texture,
	bool selected)
{
	const float nameHeight = ImGui::GetTextLineHeight();
	const float subHeight = nameHeight * 0.85f;
	const float cardHeight = CardPad + ThumbHeight + 7.0f + nameHeight + subHeight + CardPad;

	const ImVec2 position = ImGui::GetCursorScreenPos();
	const ImVec2 size(CardWidth, cardHeight);
	const ImVec2 rectMax(position.x + size.x, position.y + size.y);

	PDCardInput input;
	input.clicked = ImGui::InvisibleButton("card", size);

	const bool hovered = ImGui::IsItemHovered();
	input.doubleClicked = hovered and ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	input.rightClicked = hovered and ImGui::IsMouseClicked(ImGuiMouseButton_Right);

	ImDrawList *drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(position, rectMax, ImGui::GetColorU32(hovered ? PDTheme::CardBgHover : PDTheme::CardBg), 0.0f);

	const ImVec2 thumbMin(position.x + CardPad, position.y + CardPad);
	const ImVec2 thumbMax(rectMax.x - CardPad, position.y + CardPad + ThumbHeight);
	drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(PDTheme::ThumbBg), 0.0f);
	drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(PDTheme::CardBorder), 0.0f, 0, 1.0f);
	addImageFitted(drawList, texture, thumbMin, thumbMax, 6.0f);

	drawList->AddText(ImVec2(position.x + CardPad + 2.0f, thumbMax.y + 7.0f), ImGui::GetColorU32(PDTheme::White), name.c_str());

	if (not sub.empty())
	{
		drawList->AddText(ImVec2(position.x + CardPad + 2.0f, thumbMax.y + 7.0f + nameHeight + 2.0f), ImGui::GetColorU32(PDTheme::TextFaint), sub.c_str());
	}

	drawList->AddRect(position, rectMax, ImGui::GetColorU32(selected ? PDTheme::Accent : PDTheme::CardBorder), 0.0f, 0, selected ? 2.0f : 1.0f);

	return input;
}

PDTexture *PDMainWindow::thumbnail(std::string const &path)
{
	if (path.empty())
	{
		return nullptr;
	}

	const auto cached = m_thumbnails.find(path);

	if (cached != m_thumbnails.end())
	{
		return &cached->second;
	}

	if (m_thumbnailBudget <= 0)
	{
		return nullptr;
	}

	m_thumbnailBudget -= 1;

	return &m_thumbnails.emplace(path, PDTexture(m_host.device(), path)).first->second;
}

void PDMainWindow::drawSceneView()
{
	m_thumbnailBudget = 24;

	beginToolbar();

	if (ImGui::Button("Clear scene"))
	{
		m_scene.clear();
	}

	char summary[64];
	std::snprintf(summary, sizeof(summary), "%d across %zu packs", sceneTotalQuantity(), m_scene.size());
	toolbarSummary(summary);

	endToolbar();

	if (m_scene.empty())
	{
		ImGui::Dummy(ImVec2(0.0f, 40.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);

		char const *const line1 = "Nothing staged yet";
		char const *const line2 = "Double-click a pack in Browser to add it here.";
		float width = ImGui::CalcTextSize(line1).x;
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - width) * 0.5f);
		ImGui::TextUnformatted(line1);

		width = ImGui::CalcTextSize(line2).x;
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - width) * 0.5f);
		ImGui::TextUnformatted(line2);

		ImGui::PopStyleColor();

		return;
	}

	constexpr float rowHeight = 56.0f;
	std::size_t removeIndex = m_scene.size();

	for (std::size_t index = 0; index < m_scene.size(); index += 1)
	{
		PDSceneEntry &entry = m_scene[index];

		ImGui::PushID(static_cast<int>(index));

		const ImVec2 rowMin = ImGui::GetCursorScreenPos();
		const ImVec2 rowSize(ImGui::GetContentRegionAvail().x, rowHeight);
		const ImVec2 rowMax(rowMin.x + rowSize.x, rowMin.y + rowSize.y);

		ImDrawList *drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(PDTheme::CardBg), 0.0f);
		drawList->AddRect(rowMin, rowMax, ImGui::GetColorU32(PDTheme::CardBorder), 0.0f, 0, 1.0f);

		const ImVec2 thumbMin(rowMin.x + 8.0f, rowMin.y + 8.0f);
		const ImVec2 thumbMax(thumbMin.x + 40.0f, thumbMin.y + 40.0f);
		drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(PDTheme::ThumbBg), 0.0f);
		drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(PDTheme::CardBorder), 0.0f, 0, 1.0f);
		addImageFitted(drawList, thumbnail(entry.previewPath), thumbMin, thumbMax, 3.0f);

		drawList->AddText(ImVec2(thumbMax.x + 12.0f, rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f), ImGui::GetColorU32(PDTheme::White), entry.displayName.c_str());

		const float stepperWidth = 24.0f + 32.0f + 24.0f;
		const float editWidth = ImGui::CalcTextSize("Edit").x + 20.0f;
		const float removeWidth = ImGui::CalcTextSize("Remove").x + 24.0f;
		const float controlsWidth = stepperWidth + 10.0f + editWidth + 10.0f + removeWidth;

		ImGui::SetCursorScreenPos(ImVec2(rowMax.x - controlsWidth - 10.0f, rowMin.y + (rowHeight - 24.0f) * 0.5f));

		bool removed = false;

		if (ImGui::Button("-", ImVec2(24.0f, 24.0f)))
		{
			if (entry.quantity > 1)
			{
				entry.quantity -= 1;
			}
			else
			{
				removed = true;
			}
		}

		ImGui::SameLine(0.0f, 0.0f);

		char quantity[16];
		std::snprintf(quantity, sizeof(quantity), "%d", entry.quantity);
		const float quantityWidth = ImGui::CalcTextSize(quantity).x;
		const ImVec2 quantityPos = ImGui::GetCursorScreenPos();
		drawList->AddText(ImVec2(quantityPos.x + (32.0f - quantityWidth) * 0.5f, quantityPos.y + 4.0f), ImGui::GetColorU32(PDTheme::White), quantity);
		ImGui::Dummy(ImVec2(32.0f, 24.0f));
		ImGui::SameLine(0.0f, 0.0f);

		if (ImGui::Button("+", ImVec2(24.0f, 24.0f)))
		{
			entry.quantity += 1;
		}

		ImGui::SameLine(0.0f, 10.0f);

		if (ImGui::Button("Edit", ImVec2(editWidth, 24.0f)))
		{
			openBehaviorEditor(entry.packPath, entry.displayName);
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Edit this pack's behaviors");
		}

		ImGui::SameLine(0.0f, 10.0f);

		if (ImGui::Button("Remove", ImVec2(removeWidth, 24.0f)))
		{
			removed = true;
		}

		ImGui::SetCursorScreenPos(rowMin);
		ImGui::Dummy(ImVec2(rowSize.x, rowHeight + 8.0f));

		ImGui::PopID();

		if (removed)
		{
			removeIndex = index;
		}
	}

	if (removeIndex < m_scene.size())
	{
		removeSceneEntry(removeIndex);
	}
}

void PDMainWindow::addToScene(std::string const &displayName, PDPonyPack const &pack)
{
	for (PDSceneEntry &entry : m_scene)
	{
		if (entry.id == pack.id)
		{
			entry.quantity += 1;

			return;
		}
	}

	PDSceneEntry entry;
	entry.id = pack.id;
	entry.displayName = displayName;
	entry.previewPath = pack.previewPath;
	entry.packPath = pack.packPath;
	entry.quantity = 1;
	m_scene.push_back(std::move(entry));
}

void PDMainWindow::removeSceneEntry(std::size_t index)
{
	if (index >= m_scene.size())
	{
		return;
	}

	m_scene.erase(m_scene.begin() + static_cast<std::ptrdiff_t>(index));
}

int PDMainWindow::gridColumns(float available, float &outIndent)
{
	const int columns = std::max(1, static_cast<int>((available + CardSpacing) / (CardWidth + CardSpacing)));
	const float blockWidth = static_cast<float>(columns) * CardWidth + static_cast<float>(columns - 1) * CardSpacing;
	outIndent = std::max(0.0f, (available - blockWidth) * 0.5f);

	return columns;
}

void PDMainWindow::drawVariantModal()
{
	if (m_variantOpen)
	{
		m_variantOpen = false;
		ImGui::OpenPopup("Choose variant");
	}

	if (not beginModal("Choose variant", ImVec2(560.0f, 420.0f)))
	{
		return;
	}

	std::vector<PDPonyGroup> const &groups = m_catalog.groups();

	if (m_variantGroup < 0 or m_variantGroup >= static_cast<int>(groups.size()))
	{
		ImGui::CloseCurrentPopup();
		endModal();

		return;
	}

	PDPonyGroup const &group = groups[static_cast<std::size_t>(m_variantGroup)];
	m_thumbnailBudget = 24;

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
	ImGui::Text("%s ships %zu variants.", group.displayName.c_str(), group.variants.size());
	ImGui::PopStyleColor();
	ImGui::Dummy(ImVec2(0.0f, 10.0f));

	ImGui::BeginChild("variantGrid", ImVec2(0.0f, -46.0f), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	float indent = 0.0f;
	const int columns = gridColumns(ImGui::GetContentRegionAvail().x, indent);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(CardSpacing, CardSpacing));

	int chosen = -1;

	for (std::size_t index = 0; index < group.variants.size(); index += 1)
	{
		if (index % static_cast<std::size_t>(columns) != 0)
		{
			ImGui::SameLine();
		}
		else
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
		}

		PDPonyPack const &variant = group.variants[index];
		ImGui::PushID(static_cast<int>(index));

		PDCardInput const input = drawPonyCard(variant.id, std::string(), thumbnail(variant.previewPath), false);

		ImGui::PopID();

		if (input.clicked)
		{
			chosen = static_cast<int>(index);
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	if (ImGui::Button("Cancel", ImVec2(110.0f, 32.0f)))
	{
		ImGui::CloseCurrentPopup();
	}

	if (chosen >= 0)
	{
		applyVariant(group, group.variants[static_cast<std::size_t>(chosen)]);
		ImGui::CloseCurrentPopup();
	}

	endModal();
}

void PDMainWindow::drawTargetButton()
{
	std::string const label = m_settingsTarget.empty() ? std::string("Global defaults") : m_settingsTargetLabel;

	if (ImGui::Button(label.c_str(), ImVec2(TargetPickerWidth, 0.0f)))
	{
		m_targetPickerOpen = true;
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Choose which pack these settings apply to");
	}
}

void PDMainWindow::drawTargetModal()
{
	if (m_targetPickerOpen)
	{
		m_targetFilter[0] = '\0';
		m_targetPickerOpen = false;
		ImGui::OpenPopup("Choose target");
	}

	if (not beginModal("Choose target", ImVec2(560.0f, 460.0f)))
	{
		return;
	}

	m_thumbnailBudget = 24;

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##targetFilter", "Search packs", m_targetFilter, sizeof(m_targetFilter));
	ImGui::Dummy(ImVec2(0.0f, 10.0f));

	std::string const filter = toLower(m_targetFilter);
	std::vector<std::string> const overridden = m_app.settings().packsWithOverrides();

	std::vector<int> visible;

	if (filter.empty())
	{
		visible.push_back(-1);
	}

	for (std::size_t index = 0; index < m_targets.size(); index += 1)
	{
		if (filter.empty() or toLower(m_targets[index].label).find(filter) != std::string::npos)
		{
			visible.push_back(static_cast<int>(index));
		}
	}

	ImGui::BeginChild("targetGrid", ImVec2(0.0f, -46.0f), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	float indent = 0.0f;
	const int columns = gridColumns(ImGui::GetContentRegionAvail().x, indent);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(CardSpacing, CardSpacing));

	bool chosen = false;
	std::string chosenId;
	std::string chosenLabel;

	for (std::size_t slot = 0; slot < visible.size(); slot += 1)
	{
		if (slot % static_cast<std::size_t>(columns) != 0)
		{
			ImGui::SameLine();
		}
		else
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
		}

		const int index = visible[slot];
		ImGui::PushID(index);

		PDCardInput input;

		if (index < 0)
		{
			input = drawPonyCard("Global defaults", "every pack", nullptr, m_settingsTarget.empty());
		}
		else
		{
			PDSettingTarget const &target = m_targets[static_cast<std::size_t>(index)];
			const bool hasOverride = std::find(overridden.begin(), overridden.end(), target.id) != overridden.end();

			input = drawPonyCard(
				target.label,
				hasOverride ? "overridden" : std::string(),
				thumbnail(target.previewPath),
				m_settingsTarget == target.id);
		}

		ImGui::PopID();

		if (input.clicked)
		{
			chosen = true;

			if (index >= 0)
			{
				chosenId = m_targets[static_cast<std::size_t>(index)].id;
				chosenLabel = m_targets[static_cast<std::size_t>(index)].label;
			}
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	if (visible.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
		ImGui::TextUnformatted("No packs match that search.");
		ImGui::PopStyleColor();
	}

	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	if (ImGui::Button("Cancel", ImVec2(110.0f, 32.0f)))
	{
		ImGui::CloseCurrentPopup();
	}

	if (chosen)
	{
		m_settingsTarget = chosenId;
		m_settingsTargetLabel = chosenLabel;
		ImGui::CloseCurrentPopup();
	}

	endModal();
}

void PDMainWindow::drawFeaturesView()
{
	PDSettingsStore& store = m_app.settings();
	bool const packScope = not m_settingsTarget.empty();

	beginToolbar();
	drawTargetButton();
	ImGui::SameLine();

	if (ImGui::Button("\xE2\x86\xBB"))
	{
		m_app.reshuffle();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Reshuffle behaviors");
	}

	ImGui::SameLine();

	if (ImGui::Button("x"))
	{
		if (packScope)
		{
			store.clearOverride(m_settingsTarget);
		}
		else
		{
			store.setValue(std::string(), PDSettings());
		}

		if (not store.save())
		{
			m_diagnostics.write("Could not save feature settings");
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(packScope ? "Use global settings" : "Reset feature settings");
	}

	endToolbar();
	drawTargetModal();

	PDSettings value = store.value(m_settingsTarget);
	bool overridden = store.hasOverride(m_settingsTarget);

	if (not m_settingsTarget.empty())
	{
		if (ImGui::Checkbox("Override global settings", &overridden))
		{
			if (overridden)
			{
				store.setValue(m_settingsTarget, value);
			}
			else
			{
				store.clearOverride(m_settingsTarget);
				value = store.value(m_settingsTarget);
			}

			if (not store.save())
			{
				m_diagnostics.write("Could not save feature settings");
			}
		}
	}

	ImGui::BeginDisabled(not m_settingsTarget.empty() and not overridden);

	bool changed = ImGui::Checkbox("Movement", &value.movement);
	changed = ImGui::Checkbox("Flying", &value.flying) or changed;
	changed = ImGui::Checkbox("Extended behaviors", &value.extended) or changed;
	changed = ImGui::Checkbox("Linked behaviors", &value.linked) or changed;
	changed = ImGui::Checkbox("Interactions", &value.interactions) or changed;
	changed = ImGui::Checkbox("Dragging", &value.dragging) or changed;
	changed = ImGui::Checkbox("Names on hover", &value.names) or changed;

	ImGui::SetNextItemWidth(190.0f);
	changed = ImGui::SliderFloat("Speed", &value.speed, 0.1f, 4.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp) or changed;

	char const* const positions[] = {"Anywhere", "Ground", "Center"};
	int start = (int)value.start;
	ImGui::SetNextItemWidth(190.0f);

	if (ImGui::Combo("Spawn position", &start, positions, 3))
	{
		value.start = (PDSpawnPosition)start;
		changed = true;
	}

	ImGui::EndDisabled();

	if (changed)
	{
		store.setValue(m_settingsTarget, value);
		m_featuresDirty = true;
	}
}

void PDMainWindow::drawSettingsView()
{
	if (m_monitors.empty())
	{
		m_monitors = enumerateMonitors();
	}

	PDSettingsStore &store = m_app.settings();

	beginToolbar();

	if (ImGui::Button("Rescan displays"))
	{
		m_monitors = enumerateMonitors();
	}

	char summary[64];
	std::snprintf(summary, sizeof(summary), "%zu displays", m_monitors.size());
	toolbarSummary(summary);

	endToolbar();

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::White);
	ImGui::TextUnformatted("Walkable displays");
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
	ImGui::TextWrapped("Entities only walk on the displays you tick here. Unticking every display leaves the primary one walkable.");
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 10.0f));

	for (PDMonitor const &monitor : m_monitors)
	{
		std::string const key = "monitor." + monitor.device;
		bool walkable = store.appFlag(key, true);

		const ImVec2 position = ImGui::GetCursorScreenPos();
		const ImVec2 size(ImGui::GetContentRegionAvail().x, SettingRowHeight);
		const ImVec2 rectMax(position.x + size.x, position.y + size.y);

		ImDrawList *drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(position, rectMax, ImGui::GetColorU32(PDTheme::CardBg), 0.0f);
		drawList->AddRect(position, rectMax, ImGui::GetColorU32(PDTheme::CardBorder), 0.0f, 0, 1.0f);

		const float lineHeight = ImGui::GetTextLineHeight();
		const float firstY = position.y + (SettingRowHeight - lineHeight * 2.0f - 2.0f) * 0.5f;

		char geometry[96];
		std::snprintf(geometry, sizeof(geometry), "%s  at  %d, %d", monitor.device.c_str(), monitor.x, monitor.y);

		drawList->AddText(ImVec2(position.x + RowPad, firstY), ImGui::GetColorU32(PDTheme::White), monitor.label.c_str());
		drawList->AddText(ImVec2(position.x + RowPad, firstY + lineHeight + 2.0f), ImGui::GetColorU32(PDTheme::TextFaint), geometry);

		const float controlHeight = ImGui::GetFrameHeight();
		ImGui::SetCursorScreenPos(ImVec2(rectMax.x - RowPad - controlHeight, position.y + (SettingRowHeight - controlHeight) * 0.5f));
		ImGui::PushID(monitor.device.c_str());

		if (ImGui::Checkbox("##walkable", &walkable))
		{
			store.setAppFlag(key, walkable);
			store.save();
		}

		ImGui::PopID();
		ImGui::SetCursorScreenPos(position);
		ImGui::Dummy(ImVec2(size.x, SettingRowHeight + 4.0f));
	}

	ImGui::Dummy(ImVec2(0.0f, 18.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::White);
	ImGui::TextUnformatted("Configuration");
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
	ImGui::TextWrapped("Feature settings, pack overrides and walkable displays are stored here:");
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextFaint);
	ImGui::TextWrapped("%s", store.path().c_str());
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 10.0f));

	if (ImGui::Button("Export...", ImVec2(120.0f, 32.0f)))
	{
		exportConfig();
	}

	ImGui::SameLine();

	if (ImGui::Button("Import...", ImVec2(120.0f, 32.0f)))
	{
		importConfig();
	}

	if (not m_configStatus.empty())
	{
		ImGui::Dummy(ImVec2(0.0f, 8.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, m_configFailed ? PDTheme::Stop : PDTheme::AccentText);
		ImGui::TextWrapped("%s", m_configStatus.c_str());
		ImGui::PopStyleColor();
	}
}

void PDMainWindow::exportConfig()
{
	std::string path;

	if (not saveFileDialog(m_host.handle(), "Export Pony Dock config", "pony-dock-config.json", path))
	{
		return;
	}

	m_configFailed = not m_app.settings().exportTo(path);
	m_configStatus = m_configFailed ? ("Could not write " + path) : ("Exported to " + path);
	m_diagnostics.write(m_configStatus);
}

void PDMainWindow::importConfig()
{
	std::string path;

	if (not openFileDialog(m_host.handle(), "Import Pony Dock config", path))
	{
		return;
	}

	m_configFailed = not m_app.settings().importFrom(path);

	m_configStatus = m_configFailed ? ("Could not import " + path) : ("Imported from " + path);

	m_diagnostics.write(m_configStatus);
}

void PDMainWindow::drawLogView()
{
	std::vector<std::string> const lines = m_diagnostics.lines();
	std::vector<std::pair<std::string, int>> const repeats = m_diagnostics.repeats();

	beginToolbar();

	if (ImGui::Button("Clear"))
	{
		m_diagnostics.clear();
	}

	char summary[64];
	std::snprintf(summary, sizeof(summary), "%zu lines", lines.size());
	toolbarSummary(summary);

	endToolbar();

	if (not repeats.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
		ImGui::TextUnformatted("Repeated");
		ImGui::PopStyleColor();

		for (std::pair<std::string, int> const &entry : repeats)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::AccentText);
			ImGui::Text("%d x", entry.second);
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::TextWrapped("%s", entry.first.c_str());
		}

		ImGui::Dummy(ImVec2(0.0f, 8.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 8.0f));
	}

	const ImVec2 panelMin = ImGui::GetCursorScreenPos();
	const ImVec2 panelSize = ImGui::GetContentRegionAvail();
	const ImVec2 panelMax(panelMin.x + panelSize.x, panelMin.y + panelSize.y);

	addShadow(ImGui::GetWindowDrawList(), panelMin, panelMax);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, PDTheme::Toolbar);
	ImGui::PushStyleColor(ImGuiCol_Border, PDTheme::CardBorder);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
	ImGui::BeginChild("logLines", panelSize, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);

	if (lines.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PDTheme::TextDim);
		ImGui::TextUnformatted("Nothing logged yet.");
		ImGui::PopStyleColor();
	}

	for (std::string const &line : lines)
	{
		ImGui::TextWrapped("%s", line.c_str());
	}

	if (not lines.empty() and ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}

int PDMainWindow::sceneTotalQuantity() const
{
	int total = 0;

	for (PDSceneEntry const &entry : m_scene)
	{
		total += entry.quantity;
	}

	return total;
}
