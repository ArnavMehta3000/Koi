#include "Koi/Editor/Panels/GBufferDebugPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <algorithm>
#include <imgui.h>

namespace koi::editor
{
	void GBufferDebugPanel::OnDraw()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (ImGui::Begin("GBuffer Debug"))
		{
			GBufferPass* pass = m_renderer->GetFrameGraph().GetPass<GBufferPass>();
			if (!pass)
			{
				ImGui::TextUnformatted("No GBufferPass in the frame graph.");
				ImGui::End();
				return;
			}

			// Get outputs from the pass itself
			const std::span<const RenderPassAttachment> outputs = pass->GetOutputs();
			m_selected = std::clamp<i32>(m_selected, 0, static_cast<i32>(outputs.size()) - 1);

			if (ImGui::BeginCombo("Channel", outputs[m_selected].Name.data()))
			{
				for (i32 i = 0; i < static_cast<i32>(outputs.size()); ++i)
				{
					const bool isSelected = (i == m_selected);
					if (ImGui::Selectable(outputs[i].Name.data(), isSelected)) m_selected = i;
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			GfxDevice& device = m_renderer->GetDevice();
			const RenderPassAttachment& out = outputs[m_selected];

			if (const Texture* tex = device.Get(out.Texture); tex && tex->SRVIndex != INVALID_DESCRIPTOR)
			{
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
				ImGui::TextDisabled("'%s' is not sampleable yet (no SRV).", out.Name.data());
			}
		}
		ImGui::End();
	}
}
