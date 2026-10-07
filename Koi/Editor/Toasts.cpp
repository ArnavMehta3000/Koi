#include "Koi/Editor/Toasts.h"
#include "Koi/Editor/ImGui/EditorPalette.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <format>

namespace koi::editor
{
	namespace
	{
		constexpr f64 kLifetime = 3.5;
		constexpr f64 kFade     = 0.4;

		const char* KindIcon(ToastKind kind)
		{
			switch (kind)
			{
				case ToastKind::Success: return ICON_FA_CIRCLE_CHECK;
				case ToastKind::Warning: return ICON_FA_TRIANGLE_EXCLAMATION;
				case ToastKind::Error:   return ICON_FA_CIRCLE_XMARK;
				default:                 return ICON_FA_CIRCLE_INFO;
			}
		}

		ImVec4 KindColor(ToastKind kind)
		{
			const EditorPalette& palette = EditorPalette::Get();
			switch (kind)
			{
				case ToastKind::Success: return palette.Success;
				case ToastKind::Warning: return palette.Warning;
				case ToastKind::Error:   return palette.Error;
				default:                 return palette.Info;
			}
		}
	}

	void Toasts::Push(ToastKind kind, std::string text)
	{
		m_toasts.push_back({ kind, std::move(text), ImGui::GetTime(), m_nextId++ });
	}

	void Toasts::Draw()
	{
		const f64 now = ImGui::GetTime();
		std::erase_if(m_toasts, [&](const Toast& toast) { return now - toast.Born > kLifetime; });
		if (m_toasts.empty()) return;

		constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
			| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const f32 margin = ImGui::GetStyle().WindowPadding.x * 2.0f;
		ImVec2 corner(viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + viewport->WorkSize.y - margin);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorPalette::Get().Elevated);
		for (auto it = m_toasts.rbegin(); it != m_toasts.rend(); ++it)
		{
			const f64 left = kLifetime - (now - it->Born);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, static_cast<f32>(std::clamp(left / kFade, 0.0, 1.0)));

			ImGui::SetNextWindowPos(corner, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
			ImGui::SetNextWindowViewport(viewport->ID);
			const std::string id = std::format("##Toast{}", it->Id);
			if (ImGui::Begin(id.c_str(), nullptr, kFlags))
			{
				const ImVec4 color = KindColor(it->Kind);
				ImGui::TextColored(color, "%s", KindIcon(it->Kind));
				ImGui::SameLine();
				ImGui::TextUnformatted(it->Text.c_str());

				// Kind-coloured stripe down the left edge, like DragAxes' markers
				const ImVec2 min = ImGui::GetWindowPos();
				const ImVec2 max(min.x + 3.0f, min.y + ImGui::GetWindowHeight());
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->PushClipRect(min, max);
				drawList->AddRectFilled(min, max, ImGui::GetColorU32(color));
				drawList->PopClipRect();

				corner.y -= ImGui::GetWindowHeight() + ImGui::GetStyle().ItemSpacing.y;
			}
			ImGui::End();
			ImGui::PopStyleVar();
		}
		ImGui::PopStyleColor();
	}
}
