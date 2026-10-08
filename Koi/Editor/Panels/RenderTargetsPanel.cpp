#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include <imgui.h>
#include <format>
#include <vector>

namespace koi::editor
{
	namespace
	{
		struct Entry
		{
			std::string   Label;    // "Pass / Attachment"
			TextureHandle Texture;
		};
	}

	void RenderTargetsPanel::OnDraw()
	{
		std::vector<Entry> entries;
		for (const RGInspection::Pass& pass : m_renderer->GetRenderGraph().GetInspection().Passes)
		{
			for (const RGInspection::Output& out : pass.Writes)
			{
				if (out.Texture)
					entries.push_back(Entry{ std::format("{} / {} v{}", pass.Name, out.Name, out.Version), out.Texture });
			}
		}

		if (entries.empty())
		{
			ImGui::TextUnformatted("No pass has produced a target yet.");
			return;
		}

		size_t selected = 0;
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (entries[i].Label == m_selected) selected = i;
		}

		if (ImGui::BeginCombo("Target", entries[selected].Label.c_str()))
		{
			for (size_t i = 0; i < entries.size(); ++i)
			{
				const bool isSelected = (i == selected);
				if (ImGui::Selectable(entries[i].Label.c_str(), isSelected)) selected = i;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		m_selected = entries[selected].Label;

		GfxDevice& device = m_renderer->GetDevice();
		if (const Texture* tex = device.Get(entries[selected].Texture); tex && tex->SRVIndex != INVALID_DESCRIPTOR)
		{
			// No pass leaves its outputs readable for us, so ask for this one explicitly
			m_renderer->RequestShaderRead(entries[selected].Texture);

			ImGui::TextDisabled("%ux%u", tex->Desc.Width, tex->Desc.Height);

			const ImVec2 avail = ImGui::GetContentRegionAvail();
			const f32 aspect   = tex->Desc.Height > 0
				? static_cast<f32>(tex->Desc.Width) / static_cast<f32>(tex->Desc.Height)
				: 1.0f;

			// Fit-to-region while preserving the target's aspect ratio
			ImVec2 size = (avail.x / aspect <= avail.y)
				? ImVec2(avail.x, avail.x / aspect)
				: ImVec2(avail.y * aspect, avail.y);

			const auto gpu = device.ResourceHeap().GpuHandle(tex->SRVIndex);
			ImGui::Image(static_cast<ImTextureID>(gpu.ptr), size);
		}
		else
		{
			ImGui::TextDisabled("'%s' is not sampleable (no SRV).", entries[selected].Label.c_str());
		}
	}
}
