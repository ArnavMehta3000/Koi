#pragma once
#include "Koi/Editor/Undo/UndoTraits.h"
#include "Koi/Engine/Core/Types.h"
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace koi::editor
{
	class EditorContext;

	// Where a watched value lives. nullopt means absent,
	template <typename S>
	concept UndoSource = requires(S s, const std::optional<typename S::Value>& value)
	{
		{ s.Read() } -> std::same_as<std::optional<typename S::Value>>;
		s.Write(value);
		{ s.Identity() } -> std::convertible_to<u64>;  // same identity while a drag is held = one step
	};

	// Any movable type with Undo/Redo. Optional: Absorb(R&&) merges a newer record, Finalize() runs when it leaves history
	template <typename R>
	concept UndoRecord = std::movable<R> && requires(R r) { r.Undo(); r.Redo(); };

	// Panel-owned state with a stable address
	template <typename T>
	struct ValueAt
	{
		using Value = T;

		explicit ValueAt(T* ptr) : Ptr(ptr) {}

		[[nodiscard]] std::optional<T> Read() const { return *Ptr; }
		void Write(const std::optional<T>& value) const { if (value) *Ptr = *value; }
		[[nodiscard]] u64 Identity() const { return reinterpret_cast<u64>(Ptr); }

		T* Ptr = nullptr;
	};

	// What a watch records
	template <UndoSource S>
	struct ValueChange
	{
		S                                Source;
		std::optional<typename S::Value> Before;
		std::optional<typename S::Value> After;

		void Undo() { Apply(Before, After); }
		void Redo() { Apply(After, Before); }
		void Absorb(ValueChange&& newer) { After = std::move(newer.After); }

	private:
		// Reflected values only write back the fields this step changed, so runtime state and later edits survive
		void Apply(const std::optional<typename S::Value>& to, const std::optional<typename S::Value>& from)
		{
			if constexpr (refl::Reflected<typename S::Value>)
			{
				std::optional<typename S::Value> live = Source.Read();
				if (to && from && live)
				{
					UndoCopyChanged(*live, *to, *from);
					Source.Write(live);
					return;
				}
			}
			Source.Write(to);
		}
	};

	template <UndoSource S>
	class UndoWatch;

	// A watch whose source type is erased, for callers that only know it at runtime. Empty when not recording
	using UndoScope = std::unique_ptr<void, void (*)(void*)>;

	// Edit-mode history
	class UndoStack : public NonCopyable
	{
	public:
		explicit UndoStack(EditorContext* ctx);
		~UndoStack();

		// Diffs the source on scope exit and records a step if it changed
		template <UndoSource S>
		[[nodiscard]] UndoWatch<S> Watch(std::string_view label, S source);

		template <UndoSource S>
		[[nodiscard]] UndoScope Scope(std::string_view label, S source);

		template <UndoRecord R>
		void Record(std::string_view label, R record, u64 identity = 0)
		{
			if (!IsRecording()) return;

			if constexpr (requires { record.Absorb(std::move(record)); })
			{
				if (IRecord* open = OpenStep(identity, TypeKey<R>()))
				{
					static_cast<Model<R>*>(open)->Value.Absorb(std::move(record));
					return;
				}
			}

			Push(Step{ .Label = std::string(label), .Identity = identity, .Type = TypeKey<R>(), .Record = std::make_unique<Model<R>>(std::move(record)) });
		}

		void Undo();
		void Redo();
		void Clear();

		[[nodiscard]] bool CanUndo() const { return m_cursor > 0; }
		[[nodiscard]] bool CanRedo() const { return m_cursor < m_steps.size(); }
		[[nodiscard]] bool IsRecording() const;

		// Closes the open step once no widget is being held
		void OnTick(f32 dt);

	private:
		struct IRecord
		{
			virtual ~IRecord() = default;
			virtual void Undo() = 0;
			virtual void Redo() = 0;
			virtual void Finalize() {}
		};

		template <typename R>
		struct Model final : IRecord
		{
			explicit Model(R value) : Value(std::move(value)) {}

			void Undo() override { Value.Undo(); }
			void Redo() override { Value.Redo(); }
			void Finalize() override
			{
				if constexpr (requires { Value.Finalize(); }) Value.Finalize();
			}

			R Value;
		};

		struct Step
		{
			std::string              Label;
			u64                      Identity = 0;  // 0 never merges
			const void*              Type     = nullptr;
			std::unique_ptr<IRecord> Record;
		};

		template <typename R>
		[[nodiscard]] static const void* TypeKey()
		{
			static constexpr char kTag = 0;
			return &kTag;
		}

		// The newest step, if it's still open and came from the same source
		[[nodiscard]] IRecord* OpenStep(u64 identity, const void* type);
		void Push(Step step);

	private:
		std::vector<Step> m_steps;
		size_t            m_cursor = 0;  // steps below it are applied, the rest were undone
		bool              m_open   = false;
	};

	template <UndoSource S>
	class UndoWatch : public NonCopyable, public NonMovable
	{
	public:
		UndoWatch(UndoStack* stack, std::string_view label, S source)
			: m_stack(stack->IsRecording() ? stack : nullptr)
			, m_label(label)
			, m_source(std::move(source))
		{
			if (m_stack) m_before = m_source.Read();
		}

		~UndoWatch()
		{
			if (!m_stack) return;

			std::optional<typename S::Value> after = m_source.Read();
			if (UndoEqual(m_before, after)) return;

			const u64 identity = m_source.Identity();
			m_stack->Record(m_label, ValueChange<S>{ m_source, std::move(m_before), std::move(after) }, identity);
		}

	private:
		UndoStack*                       m_stack = nullptr;  // null when not recording
		std::string                      m_label;
		S                                m_source;
		std::optional<typename S::Value> m_before;
	};

	template <UndoSource S>
	UndoWatch<S> UndoStack::Watch(std::string_view label, S source)
	{
		return UndoWatch<S>(this, label, std::move(source));
	}

	template <UndoSource S>
	UndoScope UndoStack::Scope(std::string_view label, S source)
	{
		if (!IsRecording()) return UndoScope(nullptr, nullptr);
		return UndoScope(new UndoWatch<S>(this, label, std::move(source)), [](void* p) { delete static_cast<UndoWatch<S>*>(p); });
	}
}
