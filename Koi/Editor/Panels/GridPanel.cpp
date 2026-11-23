#include "Koi/Editor/Panels/GridPanel.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Undo/UndoStack.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Core/ReflectSerializer.h"
#include <imgui.h>

namespace koi::editor
{
	GridPanel::GridPanel(EditorContext* ctx)
		: m_renderer(&Engine::Get().GetSubsystem<Renderer>())
		, m_undo(ctx->Get<UndoStack>())
	{
	}

	GridPass* GridPanel::FindPass() const
	{
		return m_renderer ? m_renderer->GetMainSceneRenderer().GetFeatures().Find<GridPass>() : nullptr;
	}

	void GridPanel::SaveState(toml::table& state) const
	{
		if (const GridPass* pass = FindPass())
		{
			state = Serializer<GridPass::Settings>::Serialize(pass->GetSettings());
		}
	}

	void GridPanel::LoadState(const toml::table& state)
	{
		GridPass* pass = FindPass();
		if (!pass || state.empty()) return;

		if (auto settings = Serializer<GridPass::Settings>::Deserialize(state))
		{
			pass->GetSettings() = *settings;
		}
	}

	void GridPanel::OnDraw()
	{
		ZoneScoped;

		GridPass* pass = FindPass();
		if (!pass)
		{
			ImGui::TextDisabled("The active render pipeline has no grid pass.");
			return;
		}

		GridPass::Settings& s = pass->GetSettings();
		UndoWatch<ValueAt<GridPass::Settings>> watch = m_undo->Watch("Grid Settings", ValueAt(&s));

		ImGui::Checkbox("Enabled", &s.Enabled);
		ImGui::BeginDisabled(!s.Enabled);

		ImGui::SeparatorText("Spacing");
		ImGui::DragFloat("Cell Size", &s.CellSize, 0.05f, 0.01f, 1000.0f, "%.2f m");
		ImGui::SetItemTooltip("World units between minor lines.");

		ImGui::DragFloat("Thick Every", &s.ThickEvery, 0.25f, 2.0f, 100.0f, "%.0f cells");
		ImGui::SetItemTooltip("Minor lines between each major line.");

		ImGui::DragFloat("Major Width", &s.MajorLineWidth, 0.05f, 1.0f, 8.0f, "%.2f px");
		ImGui::SetItemTooltip("Width of major and axis lines. Minor lines are always 1 pixel.");

		ImGui::SeparatorText("Fade");
		ImGui::DragFloat("Fade Start", &s.FadeStart, 0.5f, 0.0f, s.FadeEnd, "%.1f m");
		ImGui::DragFloat("Fade End",   &s.FadeEnd,   0.5f, s.FadeStart, 100000.0f, "%.1f m");
		ImGui::SetItemTooltip("Distance from the camera where the grid has fully dissolved.");

		ImGui::SeparatorText("Colors");
		constexpr ImGuiColorEditFlags kColorFlags = ImGuiColorEditFlags_AlphaBar
			| ImGuiColorEditFlags_AlphaPreviewHalf;

		ImGui::ColorEdit4("Minor", &s.ThinColor.x,  kColorFlags);
		ImGui::ColorEdit4("Major", &s.ThickColor.x, kColorFlags);
		ImGui::ColorEdit4("X Axis", &s.AxisXColor.x, kColorFlags);
		ImGui::ColorEdit4("Z Axis", &s.AxisZColor.x, kColorFlags);
		ImGui::TextDisabled("Alpha is the line's peak opacity.");

		ImGui::EndDisabled();

		ImGui::Separator();
		if (ImGui::Button("Reset to Defaults"))
		{
			const bool wasEnabled = s.Enabled;
			s = GridPass::Settings{};
			s.Enabled = wasEnabled;
		}
	}
}
