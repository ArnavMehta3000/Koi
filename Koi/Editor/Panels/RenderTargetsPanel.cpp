#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
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
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (ImGui::Begin("Render Targets"))
		{
			std::vector<Entry> entries;
			for (const auto& pass : m_renderer->GetFrameGraph().GetPasses())
			{
				for (const RenderPassAttachment& out : pass->GetOutputs())
				{
					if (out.Texture)
						entries.push_back(Entry{ std::format("{} / {}", pass->GetName(), out.Name), out.Texture });
				}
			}

			if (entries.empty())
			{
				ImGui::TextUnformatted("No pass has produced a target yet.");
				ImGui::End();
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
		ImGui::End();
	}
}
