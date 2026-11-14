#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace koi::editor
{
	namespace
	{
		constexpr const char* kDragDropPayloadType = "KOI_SCENE_ENTITY";

		const char* EntityIcon(Entity e)
		{
			if (e.Has<CameraComponent>()) return ICON_FA_VIDEO;
			if (e.Has<Light>())           return ICON_FA_LIGHTBULB;
			if (e.Has<MeshRef>())         return ICON_FA_SHAPES;
			return ICON_FA_CUBE;
		}

		// Appends the children of `parent` (roots when invalid) in outliner order, returns where they start
		size_t AppendSortedChildren(ECS& ecs, Entity parent, std::vector<Entity>& out)
		{
			const size_t first = out.size();
			if (parent.IsAlive())
			{
				std::ranges::copy(parent.Children(), std::back_inserter(out));
			}
			else
			{
				ecs.Each<const Transform>([&](Entity e, const Transform&) { if (!e.GetParent()) out.push_back(e); });
			}

			std::ranges::sort(out.begin() + first, out.end(), {}, [](Entity e) { return World::OrderKey(e); });
			return first;
		}

		// Calls `fn` on each sorted child; indexed since `fn` may append below them
		template <typename Fn>
		void ForEachSortedChild(ECS& ecs, Entity parent, std::vector<Entity>& scratch, Fn&& fn)
		{
			const size_t first = AppendSortedChildren(ecs, parent, scratch);
			const size_t last  = scratch.size();
			for (size_t i = first; i < last; ++i) fn(scratch[i]);
			scratch.resize(first);
		}

		// Depth-first renumber after any structural edit
		void Renumber(ECS& ecs, std::vector<Entity>& scratch)
		{
			u32 next = 0;
			auto visit = [&](this auto&& self, Entity parent) -> void
			{
				ForEachSortedChild(ecs, parent, scratch, [&](Entity e)
				{
					e.Set<SceneOrder>({ next++ });
					self(e);
				});
			};
			visit(Entity{});
		}
	}

	WorldOutlinePanel::WorldOutlinePanel(EditorContext* ctx)
		: m_game(ctx->GetGame())
		, m_selection(ctx->Get<Selection>())
	{
	}

	void WorldOutlinePanel::OnDraw()
	{
		ECS& ecs = m_game->GetWorld().GetECS();

		// Structural edits are collected and applied after the draw
		struct PendingDrop { Entity Dragged; Entity Target; enum { Before, Into, After } Where; };
		std::optional<PendingDrop> pendingDrop;
		bool renumber = false;

		const Entity selected = m_selection->GetEntity();

		auto DrawNode = [&](this auto&& self, Entity entity) -> void
		{
			const bool hasChildren = entity.HasChildren();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
										| ImGuiTreeNodeFlags_SpanAvailWidth
										| ImGuiTreeNodeFlags_DefaultOpen
										| ImGuiTreeNodeFlags_DrawLinesToNodes;

			if (!hasChildren)
				flags |= ImGuiTreeNodeFlags_Leaf;

			if (selected.IsAlive() && selected.GetId() == entity.GetId())
				flags |= ImGuiTreeNodeFlags_Selected;

			const std::string name(entity.GetName());
			const char* label = name.empty() ? "<unnamed>" : name.c_str();
			void* nodeId = reinterpret_cast<void*>(static_cast<intptr_t>(entity.GetId()));

			const bool renamingThis = m_renaming == entity.GetId();
			bool open;

			if (renamingThis)
			{
				open = ImGui::TreeNodeEx(nodeId, flags, "");
				ImGui::SameLine();

				if (m_focusRename)
				{
					ImGui::SetKeyboardFocusHere();
					m_focusRename = false;
				}

				ImGui::SetNextItemWidth(160.0f);
				const bool commit = ImGui::InputText("##rename", &m_renameBuffer,
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

				if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					m_renaming = 0;
				}
				else if (commit || ImGui::IsItemDeactivatedAfterEdit())
				{
					if (!m_renameBuffer.empty())
						entity.SetName(m_renameBuffer);
					m_renaming = 0;
				}
				else if (ImGui::IsItemDeactivated())
				{
					m_renaming = 0;
				}
			}
			else
			{
				open = ImGui::TreeNodeEx(nodeId, flags, "%s  %s", EntityIcon(entity), label);

				if (ImGui::IsItemClicked())
					m_selection->Select(entity);

				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
				{
					m_renaming     = entity.GetId();
					m_renameBuffer = label;
					m_focusRename  = true;
				}

				if (ImGui::BeginDragDropSource())
				{
					EntityID id = entity.GetId();
					ImGui::SetDragDropPayload(kDragDropPayloadType, &id, sizeof(id));
					ImGui::TextUnformatted(label);
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginDragDropTarget())
				{
					// Top / bottom quarter of the row inserts as a sibling; the middle reparents
					const ImVec2 min = ImGui::GetItemRectMin();
					const ImVec2 max = ImGui::GetItemRectMax();
					const f32    t   = (ImGui::GetMousePos().y - min.y) / std::max(max.y - min.y, 1.0f);
					const auto   where = t < 0.25f ? PendingDrop::Before : t > 0.75f ? PendingDrop::After : PendingDrop::Into;

					if (where != PendingDrop::Into)
					{
						const f32 y = where == PendingDrop::Before ? min.y : max.y;
						ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
					}

					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropPayloadType, where != PendingDrop::Into ? ImGuiDragDropFlags_AcceptNoDrawDefaultRect : 0))
					{
						const Entity dragged = ecs.FromId(*static_cast<const EntityID*>(payload->Data));
						if (dragged.IsAlive() && dragged != entity)
							pendingDrop = PendingDrop{ dragged, entity, where };
					}
					ImGui::EndDragDropTarget();
				}
			}

			if (open)
			{
				ForEachSortedChild(ecs, entity, m_order, self);
				ImGui::TreePop();
			}
		};

		// F2 renames the current selection wherever it ends up in the tree below
		if (selected.IsAlive() && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
		{
			m_renaming     = selected.GetId();
			m_renameBuffer = selected.GetName();
			m_focusRename  = true;
		}

		if (utils::IconButton(ICON_FA_PLUS, "Add entity (as a child of the selection)"))
		{
			const Entity e = ecs.Spawn();
			e.SetName("Entity_" + std::to_string(e.GetId()));
			e.Set<Transform>({});
			if (selected.IsAlive())
				e.SetParent(selected);
			m_selection->Select(e);
			renumber = true;
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!selected.IsAlive());

		if (utils::IconButton(ICON_FA_TRASH, "Remove selected entity"))
		{
			m_selection->Clear();
			selected.Destroy();
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		ForEachSortedChild(ecs, Entity{}, m_order, DrawNode);

		if (pendingDrop)
		{
			auto& [dragged, target, where] = *pendingDrop;
			if (where == PendingDrop::Into)
			{
				dragged.SetParent(target);
				dragged.Set<SceneOrder>({ ~0u });  // last among its new siblings until renumbered
			}
			else
			{
				// Same parent as the target, then slot in just before/after it. Fractional slots
				// aren't possible on u32, so shift the target and everything past it by one first
				dragged.SetParent(target.GetParent());

				const u32 slot = target.TryGet<const SceneOrder>() ? target.Get<const SceneOrder>().Index + (where == PendingDrop::After ? 1 : 0) : 0;
				ecs.Each<SceneOrder>([&](Entity e, SceneOrder& order) { if (e != dragged && order.Index >= slot) ++order.Index; });
				dragged.Set<SceneOrder>({ slot });
			}
			renumber = true;
		}

		if (renumber) Renumber(ecs, m_order);
	}
}
