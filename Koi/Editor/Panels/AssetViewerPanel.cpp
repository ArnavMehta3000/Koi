#include "Koi/Editor/Panels/AssetViewerPanel.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/SceneRenderer.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <utility>

namespace koi::editor
{
	AssetViewerPanel::AssetViewerPanel(EditorContext* ctx)
		: m_selection(ctx->Get<Selection>())
	{
	}

	AssetViewerPanel::~AssetViewerPanel() = default;

	void AssetViewerPanel::OnTick(f32 dt)
	{
		// Camera only: a preview never gives the game input focus
		const InputManager& input = Engine::Get().GetSubsystem<InputManager>();
		UpdateCapture(input, false);
		UpdateCamera(dt, input, false);
	}

	void AssetViewerPanel::FollowSelection()
	{
		if (!m_selection->HasAsset() || m_selection->GetAsset() == m_shown) return;

		m_shown = m_selection->GetAsset();
		const AssetTypeInfo* type = koi::utils::FindAssetTypeForFile(m_shown);
		const AssetKind kind = type ? type->Kind : AssetKind::Unknown;

		switch (kind)
		{
			case AssetKind::Mesh:     m_preview.ShowMesh(m_shown);     break;
			case AssetKind::Material: m_preview.ShowMaterial(m_shown); break;
			default:                  m_preview.Clear();               break;
		}

		m_unsupported = (kind == AssetKind::Mesh || kind == AssetKind::Material) ? std::string_view{} : (type ? type->Name : "this asset");
		m_framePending = m_unsupported.empty();
	}

	void AssetViewerPanel::OnRender()
	{
		ZoneScoped;
		FollowSelection();
		m_preview.Update();

		if (std::exchange(m_framePending, false))
		{
			FrameBounds(m_preview.GetBounds());
		}

		const ViewportTarget* target = PrepareFrame();
		if (!target) return;

		if (!m_sceneRenderer)
		{
			m_sceneRenderer = std::make_unique<SceneRenderer>(*m_renderer, "Deferred");
		}
		m_sceneRenderer->Submit(RenderView{ .Camera = &m_camera, .Target = *target }, m_preview.GetRenderScene());
	}

	void AssetViewerPanel::OnWindowGui()
	{
		if (m_preview.HasSubject()) return;

		// Window-relative, so measured from where content starts: the docked tab bar sits above it
		const ImVec2 origin = ImGui::GetCursorStartPos();
		ImGui::SetCursorPos(ImVec2(origin.x + 12.0f, origin.y + 12.0f));
		if (m_unsupported.empty())
		{
			ImGui::TextDisabled("Select a mesh or material in the Assets panel");
		}
		else
		{
			ImGui::TextDisabled("No 3D preview for %.*s; select a mesh or material", static_cast<int>(m_unsupported.size()), m_unsupported.data());
		}
	}
}
