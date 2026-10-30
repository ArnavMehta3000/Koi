#include "Koi/Editor/Undo/UndoStack.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/PlaySession.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <ImGuizmo.h>

namespace koi::editor
{
	namespace
	{
		constexpr size_t kMaxSteps = 100;
	}

	UndoStack::UndoStack(EditorContext* ctx)
	{
		// Stop rebuilds the world and recycles entity ids, so nothing recorded before Play still points at the right thing
		PlaySession* play = ctx->Get<PlaySession>();
		play->OnStarted([this] { m_open = false; });
		play->OnStopped([this] { Clear(); });
	}

	UndoStack::~UndoStack()
	{
		Clear();
	}

	bool UndoStack::IsRecording() const
	{
		return Engine::Get().GetPlayState() == PlayState::Edit;
	}

	void UndoStack::OnTick(f32)
	{
		// ImGuizmo never takes an ImGui active id, so a held gizmo has to be checked on its own
		if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing()) m_open = false;
	}

	UndoStack::IRecord* UndoStack::OpenStep(u64 identity, const void* type)
	{
		if (!m_open || identity == 0 || m_cursor == 0 || m_cursor != m_steps.size()) return nullptr;

		Step& newest = m_steps.back();
		return newest.Identity == identity && newest.Type == type ? newest.Record.get() : nullptr;
	}

	void UndoStack::Push(Step step)
	{
		// A new edit after an undo drops the steps that could have been redone
		while (m_steps.size() > m_cursor)
		{
			m_steps.back().Record->Finalize();
			m_steps.pop_back();
		}

		if (m_steps.size() == kMaxSteps)
		{
			m_steps.front().Record->Finalize();
			m_steps.erase(m_steps.begin());
		}

		KOI_LOG(Editor, Trace, "Undo step '{}'", step.Label);
		m_steps.push_back(std::move(step));
		m_cursor = m_steps.size();
		m_open   = true;
	}

	void UndoStack::Undo()
	{
		m_open = false;
		if (!CanUndo()) return;

		Step& step = m_steps[--m_cursor];
		step.Record->Undo();
		KOI_LOG(Editor, Debug, "Undo '{}'", step.Label);
	}

	void UndoStack::Redo()
	{
		m_open = false;
		if (!CanRedo()) return;

		Step& step = m_steps[m_cursor++];
		step.Record->Redo();
		KOI_LOG(Editor, Debug, "Redo '{}'", step.Label);
	}

	void UndoStack::Clear()
	{
		while (!m_steps.empty())
		{
			m_steps.back().Record->Finalize();
			m_steps.pop_back();
		}
		m_cursor = 0;
		m_open   = false;
	}
}
