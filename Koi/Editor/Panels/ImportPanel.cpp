#include "Koi/Editor/Panels/ImportPanel.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Assets/ImportQueue.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <format>
#include <utility>

namespace koi::editor
{
	namespace
	{
		bool KindCombo(const char* id, TextureKind& kind)
		{
			bool changed = false;
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo(id, ToString(kind).data()))
			{
				for (u8 i = 0; i < static_cast<u8>(TextureKind::COUNT); ++i)
				{
					const TextureKind option = static_cast<TextureKind>(i);
					if (ImGui::Selectable(ToString(option).data(), option == kind))
					{
						kind = option;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		constexpr ImGuiTableFlags kPendingTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

		f32 TableHeight(size_t rows)
		{
			return std::min(ImGui::GetTextLineHeightWithSpacing() * (static_cast<f32>(rows) + 2.0f), 320.0f);
		}

		bool Toggle(const char* id, u8& value)
		{
			bool on = value != 0;
			const bool changed = ImGui::Checkbox(id, &on);
			value = on ? 1 : 0;
			return changed;
		}
	}

	void ImportPanel::OnUpdate()
	{
		if (std::exchange(m_focusPending, false))
			ImGui::SetNextWindowFocus();
	}

	void ImportPanel::OnDraw()
	{
		ZoneScoped;

		if (m_queue.Pending().empty() && m_queue.History().empty())
		{
			ImGui::TextDisabled("Drag images or glTF models from Explorer onto the editor window to import them");
			ImGui::TextDisabled("PNG, JPG, TGA, BMP, TIFF, DDS and HDR all valid");
		}

		if (!m_queue.Pending().empty())
			DrawPending();

		if (!m_queue.History().empty())
		{
			ImGui::SeparatorText("Progress");
			DrawProgress();
		}
	}

	void ImportPanel::DrawDestination(const char* id, std::string& folder)
	{
		ImGui::PushID(id);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Into");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##Destination", folder.c_str(), ImGuiComboFlags_HeightLarge))
		{
			for (const std::string& option : m_database.Folders())
			{
				if (!option.starts_with("project://")) continue;  // engine content is read-only
				if (ImGui::Selectable(option.c_str(), option == folder)) folder = option;
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("Folders that don't exist yet are created. Type a new one below.");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputText("##DestinationText", &folder);
		ImGui::PopID();
	}

	void ImportPanel::DrawPending()
	{
		std::vector<std::shared_ptr<ImportItem>>& pending = m_queue.Pending();
		const auto count = [&](AssetFileType type) { return std::ranges::count(pending, type, [](const auto& item) { return item->Type; }); };

		size_t removeAt = pending.size();
		if (count(AssetFileType::Texture) > 0) DrawTextureTable(removeAt);
		if (count(AssetFileType::Mesh) > 0)    DrawModelTable(removeAt);

		if (removeAt < pending.size()) pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(removeAt));

		if (ImGui::Button(std::format("Import {} file(s)", pending.size()).c_str()))
			m_queue.StartPending();
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			m_queue.ClearPending();
	}

	void ImportPanel::DrawTextureTable(size_t& removeAt)
	{
		std::vector<std::shared_ptr<ImportItem>>& pending = m_queue.Pending();
		ImGui::SeparatorText("Images");
		DrawDestination("Images", m_queue.DestinationFolder());

		// Set every row at once, the usual case for a folder of one kind of texture
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Make all");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(140.0f);
		if (KindCombo("##AllKind", m_allKind))
		{
			for (const auto& item : pending)
			{
				item->Texture.Kind = m_allKind;
			}
		}

		if (!ImGui::BeginTable("##PendingTextures", 6, kPendingTableFlags, ImVec2(0.0f, TableHeight(pending.size()))))
			return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("File",     ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("Kind",     ImGuiTableColumnFlags_WidthStretch, 1.5f);
		ImGui::TableSetupColumn("Mips",     ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Compress", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Output",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < pending.size(); ++i)
		{
			ImportItem& item = *pending[i];
			if (item.Type != AssetFileType::Texture) continue;

			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(item.Source.filename().string().c_str());
			ImGui::SetItemTooltip("%s", item.Source.string().c_str());

			ImGui::TableNextColumn();
			KindCombo("##Kind", item.Texture.Kind);

			ImGui::TableNextColumn();
			Toggle("##Mips", item.Texture.GenerateMips);

			ImGui::TableNextColumn();
			Toggle("##Compress", item.Texture.Compress);

			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("%s", DescribeFormat(OutputFormatFor(item.Texture)).data());

			ImGui::TableNextColumn();
			if (ImGui::SmallButton("x")) removeAt = i;
			ImGui::SetItemTooltip("Leave this one out");

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	void ImportPanel::DrawModelTable(size_t& removeAt)
	{
		std::vector<std::shared_ptr<ImportItem>>& pending = m_queue.Pending();
		ImGui::SeparatorText("Models");
		DrawDestination("Models", m_queue.ModelDestinationFolder());

		if (!ImGui::BeginTable("##PendingModels", 4, kPendingTableFlags, ImVec2(0.0f, TableHeight(pending.size()))))
			return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("File",      ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("Materials", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Scale",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("",          ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < pending.size(); ++i)
		{
			ImportItem& item = *pending[i];
			if (item.Type != AssetFileType::Mesh) continue;

			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(item.Source.filename().string().c_str());
			ImGui::SetItemTooltip("%s\nLands in its own folder: <name>.kmesh plus a .kmat per material and a .ktex per texture", item.Source.string().c_str());

			ImGui::TableNextColumn();
			Toggle("##Materials", item.Model.ImportMaterials);
			ImGui::SetItemTooltip("Bring the materials and their textures along. Off leaves the model on the default material");

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Scale", &item.Model.Scale, 0.01f, 0.001f, 1000.0f, "%.3f");
			ImGui::SetItemTooltip("Baked into the vertices. 0.01 turns centimetres into metres");

			ImGui::TableNextColumn();
			if (ImGui::SmallButton("x")) removeAt = i;
			ImGui::SetItemTooltip("Leave this one out");

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	void ImportPanel::DrawProgress()
	{
		const std::span<const std::shared_ptr<ImportItem>> history = m_queue.History();

		if (ImGui::BeginTable("##Progress", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("File",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed);

			for (size_t i = 0; i < history.size(); ++i)
			{
				ImportItem& item = *history[i];
				const ImportStatus status = item.Status.load(std::memory_order_acquire);
				ImGui::PushID(static_cast<int>(i));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(item.Source.filename().string().c_str());
				ImGui::SetItemTooltip("-> %s", item.OutputPath.c_str());

				ImGui::TableNextColumn();
				switch (status)
				{
					case ImportStatus::Waiting:
						ImGui::ProgressBar(0.0f, ImVec2(-FLT_MIN, 0.0f), "Waiting");
						break;
					case ImportStatus::Running:
						ImGui::ProgressBar(item.Progress.Fraction(), ImVec2(-FLT_MIN, 0.0f), item.Progress.Stage().c_str());
						break;
					case ImportStatus::Done:
						ImGui::ProgressBar(1.0f, ImVec2(-FLT_MIN, 0.0f), std::format("Done in {:.1f} s", item.Seconds).c_str());
						ImGui::SetItemTooltip("%s", item.Progress.Breakdown().c_str());
						break;
					case ImportStatus::Failed:
						ImGui::AlignTextToFramePadding();
						ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1.0f), "Failed: %s", item.Error.c_str());
						break;
					case ImportStatus::Cancelled:
						ImGui::AlignTextToFramePadding();
						ImGui::TextDisabled("Cancelled");
						break;
				}

				ImGui::TableNextColumn();
				const bool active = status == ImportStatus::Waiting || status == ImportStatus::Running;
				ImGui::BeginDisabled(!active || item.Progress.IsCancelled());
				if (ImGui::SmallButton("Cancel")) item.Progress.RequestCancel();
				ImGui::EndDisabled();

				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		if (ImGui::Button("Cancel all")) m_queue.CancelAll();
		ImGui::SameLine();
		if (ImGui::Button("Clear finished")) m_queue.ClearFinished();
	}
}
