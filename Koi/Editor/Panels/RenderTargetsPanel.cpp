#include "Koi/Editor/Panels/RenderTargetsPanel.h"
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <format>

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kCapturePass = "Capture";

		bool IsDepth(Format format)
		{
			return format == Format::D32_FLOAT || format == Format::D32_FLOAT_S8X24_UINT;
		}

		// What a fresh pick should look like: depth as distance, everything else as plain color
		CaptureSettings DefaultsFor(Format format)
		{
			return IsDepth(format)
				? CaptureSettings{ .Mode = CaptureMode::LinearDepth, .Channels = 0b0001 }
				: CaptureSettings{};
		}

		const RGInspection::Output* FindWatched(const RGInspection& inspection, const TargetCapture::Target& target)
		{
			for (const RGInspection::Pass& pass : inspection.Passes)
			{
				for (const RGInspection::Output& out : pass.Writes)
				{
					if (out.Name == target.Name && out.Version == target.Version)
						return &out;
				}
			}
			return nullptr;
		}
	}

	RenderTargetsPanel::RenderTargetsPanel()
		: m_renderer(&Engine::Get().GetSubsystem<Renderer>())
	{
	}

	void RenderTargetsPanel::OnDraw()
	{
		constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
		if (!ImGui::BeginTable("##RenderTargets", 2, kFlags)) return;

		ImGui::TableSetupColumn("Targets", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		DrawTargetList();

		ImGui::TableNextColumn();
		DrawPreview();

		ImGui::EndTable();
	}

	void RenderTargetsPanel::DrawTargetList()
	{
		TargetCapture& capture = m_renderer->GetTargetCapture();
		const auto&    watched = capture.GetWatched();
		GfxDevice&     device  = m_renderer->GetDevice();

		ImGui::BeginChild("##List");
		for (const RGInspection::Pass& pass : m_renderer->GetRenderGraph().GetInspection().Passes)
		{
			if (pass.Name == kCapturePass || pass.Writes.empty()) continue;

			ImGui::SeparatorText(pass.Name.data());
			for (const RGInspection::Output& out : pass.Writes)
			{
				const bool selected = watched && watched->Name == out.Name && watched->Version == out.Version;
				const std::string label = std::format("{} v{}##{}", out.Name, out.Version, pass.Name);

				if (ImGui::Selectable(label.c_str(), selected) && !selected)
				{
					capture.Watch(TargetCapture::Target{ .Name = std::string(out.Name), .Version = out.Version });
					if (const Texture* tex = device.Get(out.Texture))
					{
						capture.GetSettings() = DefaultsFor(tex->Desc.Format);
					}
				}
			}
		}
		ImGui::EndChild();
	}

	void RenderTargetsPanel::DrawPreview()
	{
		TargetCapture& capture = m_renderer->GetTargetCapture();
		const auto&    watched = capture.GetWatched();
		if (!watched)
		{
			ImGui::TextDisabled("Pick a target on the left.");
			return;
		}

		CaptureSettings& s = capture.GetSettings();

		int mode = static_cast<int>(s.Mode);
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("##Mode", &mode, "Color\0Linear Depth\0"))
			s.Mode = static_cast<CaptureMode>(mode);

		static constexpr const char* kChannels[] = { "R", "G", "B", "A" };
		for (u32 i = 0; i < 4; ++i)
		{
			ImGui::SameLine();
			bool on = (s.Channels >> i) & 1u;
			if (ImGui::Checkbox(kChannels[i], &on))
				s.Channels = on ? (s.Channels | (1u << i)) : (s.Channels & ~(1u << i));
		}

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloatRange2("##Range", &s.RangeMin, &s.RangeMax, 0.01f, 0.0f, 0.0f, "Min %.2f", "Max %.2f");

		GfxDevice& device = m_renderer->GetDevice();
		if (const RGInspection::Output* out = FindWatched(m_renderer->GetRenderGraph().GetInspection(), *watched))
		{
			if (const Texture* tex = device.Get(out->Texture))
			{
				ImGui::TextDisabled("%ux%u  %s", tex->Desc.Width, tex->Desc.Height, std::string(refl::EnumName<Format, 192>(tex->Desc.Format)).c_str());
			}
		}

		const TextureHandle preview = capture.GetPreview();
		const Texture*      tex     = device.Get(preview);
		if (!tex)
		{
			ImGui::TextDisabled("'%s v%u' was not produced this frame.", watched->Name.c_str(), watched->Version);
			return;
		}

		m_renderer->RequestShaderRead(preview);

		// Fit to the region, keeping the target's aspect
		const ImVec2 avail  = ImGui::GetContentRegionAvail();
		const f32    aspect = static_cast<f32>(tex->Desc.Width) / static_cast<f32>(std::max(tex->Desc.Height, 1u));
		const ImVec2 size   = (avail.x / aspect <= avail.y)
			? ImVec2(avail.x, avail.x / aspect)
			: ImVec2(avail.y * aspect, avail.y);

		const auto gpu = device.ResourceHeap().GpuHandle(tex->SRVIndex);
		ImGui::Image(static_cast<ImTextureID>(gpu.ptr), size);
	}
}
