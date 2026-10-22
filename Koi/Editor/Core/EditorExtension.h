#pragma once
#include "Koi/Engine/Core/Macros.h"
#include <memory>
#include <span>
#include <string_view>

namespace koi::editor
{
	class EditorContext;

	// Editor-only code for a game or an engine feature
	class EditorExtension
	{
	public:
		virtual ~EditorExtension() = default;

		virtual void OnStartup(EditorContext& ctx) = 0;
		virtual void OnShutdown() {}
	};

	class EditorExtensionRegistry
	{
	public:
		using Factory = std::unique_ptr<EditorExtension> (*)();

		struct Entry
		{
			std::string_view Name;
			Factory          Create;
		};

		static bool Add(std::string_view name, Factory factory);
		[[nodiscard]] static std::span<const Entry> All();
	};
}

#define KOI_INTERNAL_EDITOR_EXTENSION(Type, Id)                                                              \
	namespace                                                                                                \
	{                                                                                                        \
		[[maybe_unused]] const bool Id = ::koi::editor::EditorExtensionRegistry::Add(#Type,                  \
			[]() -> std::unique_ptr<::koi::editor::EditorExtension> { return std::make_unique<Type>(); });   \
	}

// Registers an EditorExtension at static-init time
#define KOI_EDITOR_EXTENSION(Type) KOI_INTERNAL_EDITOR_EXTENSION(Type, KOI_CONCAT(s_editorExtension, __COUNTER__))
