#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <optional>
#include <vector>

namespace koi::editor
{
	namespace
	{
		constexpr const char* kDragDropPayloadType = "KOI_SCENE_ENTITY";

		const char* EntityIcon(Entity e)
		{
			if (e.has<CameraComponent>()) return ICON_FA_VIDEO;
			if (e.has<Light>())           return ICON_FA_LIGHTBULB;
			if (e.has<MeshRef>())         return ICON_FA_SHAPES;
			return ICON_FA_CUBE;
		}

		// Children of `parent` (roots when invalid) in outliner order
		std::vector<Entity> SortedChildren(ECS& ecs, Entity parent)
		{
			std::vector<Entity> out;
			if (parent.is_alive())
			{
				parent.children([&](Entity e) { out.push_back(e); });
			}
			else
			{
				ecs.each<Transform>([&](Entity e, Transform&) { if (!e.parent()) out.push_back(e); });
			}

			std::ranges::sort(out, {}, [](Entity e) { return World::OrderKey(e); });
			return out;
		}

		// Depth-first renumber after any structural edit
		void Renumber(ECS& ecs)
		{
			u32 next = 0;
			auto visit = [&](this auto&& self, Entity parent) -> void
			{
				for (Entity e : SortedChildren(ecs, parent))
				{
					e.set<SceneOrder>({ next++ });
					self(e);
				}
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
			bool hasChildren = false;
			entity.children([&](Entity) { hasChildren = true; });

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
										| ImGuiTreeNodeFlags_SpanAvailWidth
										| ImGuiTreeNodeFlags_DefaultOpen
										| ImGuiTreeNodeFlags_DrawLinesToNodes;

			if (!hasChildren)
				flags |= ImGuiTreeNodeFlags_Leaf;

			if (selected.is_alive() && selected.id() == entity.id())
				flags |= ImGuiTreeNodeFlags_Selected;

			const char* label = entity.name().length() > 0 ? entity.name().c_str() : "<unnamed>";
			void* nodeId = reinterpret_cast<void*>(static_cast<intptr_t>(entity.id()));

			const bool renamingThis = m_renaming == entity.id();
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
						entity.set_name(m_renameBuffer.c_str());
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
					m_renaming     = entity.id();
					m_renameBuffer = label;
					m_focusRename  = true;
				}

				if (ImGui::BeginDragDropSource())
				{
					EntityID id = entity.id();
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
						Entity dragged = ecs.entity(*static_cast<EntityID*>(payload->Data));
						if (dragged.is_alive() && dragged != entity)
							pendingDrop = PendingDrop{ dragged, entity, where };
					}
					ImGui::EndDragDropTarget();
				}
			}

			if (open)
			{
				for (Entity child : SortedChildren(ecs, entity)) self(child);
				ImGui::TreePop();
			}
		};

		// F2 renames the current selection wherever it ends up in the tree below
		if (selected.is_alive() && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
		{
			m_renaming     = selected.id();
			m_renameBuffer = selected.name().length() > 0 ? selected.name().c_str() : "";
			m_focusRename  = true;
		}

		if (utils::IconButton(ICON_FA_PLUS, "Add entity (as a child of the selection)"))
		{
			Entity e = ecs.entity();
			e.set_name(("Entity_" + std::to_string(static_cast<u64>(e.id()))).c_str());
			e.set<Transform>({});
			if (selected.is_alive())
				e.child_of(selected);
			m_selection->Select(e);
			renumber = true;
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!selected.is_alive());

		if (utils::IconButton(ICON_FA_TRASH, "Remove selected entity"))
		{
			m_selection->Clear();
			selected.destruct();
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		for (Entity root : SortedChildren(ecs, Entity{})) DrawNode(root);

		if (pendingDrop)
		{
			auto& [dragged, target, where] = *pendingDrop;
			if (where == PendingDrop::Into)
			{
				dragged.child_of(target);
				dragged.set<SceneOrder>({ ~0u });  // last among its new siblings until renumbered
			}
			else
			{
				// Same parent as the target, then slot in just before/after it. Fractional slots
				// aren't possible on u32, so shift the target and everything past it by one first
				if (const Entity parent = target.parent())
					dragged.child_of(parent);
				else
					dragged.remove(flecs::ChildOf, flecs::Wildcard);

				const u32 slot = target.try_get<SceneOrder>() ? target.get<SceneOrder>().Index + (where == PendingDrop::After ? 1 : 0) : 0;
				ecs.each<SceneOrder>([&](Entity e, SceneOrder& order) { if (e != dragged && order.Index >= slot) ++order.Index; });
				dragged.set<SceneOrder>({ slot });
			}
			renumber = true;
		}

		if (renumber) Renumber(ecs);
	}
}
