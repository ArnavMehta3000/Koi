#include "Koi/Editor/SceneFiles.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Editor/Toasts.h"
#include "Koi/Editor/Undo/UndoStack.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <format>
#include <fstream>
#include <utility>

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kNewSceneTemplate = R"([[entity]]
id = 0
name = 'Camera'

    [entity.Camera]
    far = 1000.0
    fov_y = 0.9
    near = 0.05
    primary = true

    [entity.Transform]
    position = [ 0.0, 2.0, 8.0 ]
    rotation = [ 0.0, 0.0, 0.0, 1.0 ]
    scale = [ 1.0, 1.0, 1.0 ]

[[entity]]
id = 1
name = 'Sun'

    [entity.Light]
    casts_shadows = true
    color = [ 1.0, 0.95, 0.9 ]
    intensity = 3.0
    range = 0.0
    shadow_bias = 0.0015
    shadow_extent = 20.0
    type = 'Directional'

    [entity.Transform]
    position = [ 0.0, 10.0, 0.0 ]
    rotation = [ -0.5, 0.3, 0.15, 0.8 ]
    scale = [ 1.0, 1.0, 1.0 ]
)";
	}

	SceneFiles::SceneFiles(EditorContext* ctx)
		: m_ctx(ctx)
		, m_database(ctx->Get<AssetDatabase>())
		, m_selection(ctx->Get<Selection>())
		, m_toasts(ctx->Get<Toasts>())
		, m_undo(ctx->Get<UndoStack>())
	{
	}

	void SceneFiles::Open(std::string virtualPath)
	{
		if (Engine::Get().GetPlayState() != PlayState::Edit)
		{
			m_toasts->Push(ToastKind::Warning, "Stop play before opening a scene");
			return;
		}
		m_pending = std::move(virtualPath);
	}

	void SceneFiles::Create(std::string_view name)
	{
		if (name.empty() || name.find_first_of("\\/:*?\"<>|.") != std::string_view::npos)
		{
			m_toasts->Push(ToastKind::Error, std::format("'{}' isn't a valid scene name", name));
			return;
		}

		const std::string virtualPath = std::format("project://Content/Scenes/{}.scene.toml", name);
		const std::optional<fs::path> diskPath = VirtualFileSystem::Get().TryResolveToDisk(virtualPath);
		if (!diskPath)
		{
			m_toasts->Push(ToastKind::Error, "The project's content isn't on disk");
			return;
		}
		if (fs::exists(*diskPath))
		{
			m_toasts->Push(ToastKind::Error, std::format("Scene '{}' already exists", name));
			return;
		}

		std::error_code ec;
		fs::create_directories(diskPath->parent_path(), ec);
		if (std::ofstream out(*diskPath, std::ios::binary); !(out << kNewSceneTemplate))
		{
			m_toasts->Push(ToastKind::Error, std::format("Couldn't write '{}'", diskPath->string()));
			return;
		}

		KOI_LOG(Editor, Info, "Created scene '{}'", virtualPath);
		m_database->Refresh();
		Open(virtualPath);
	}

	void SceneFiles::OnTick(f32)
	{
		if (m_pending.empty()) return;

		ZoneScoped;
		const std::string path = std::exchange(m_pending, {});
		if (const Result<void> loaded = m_ctx->GetGame()->GetWorld().Load(path); !loaded)
		{
			KOI_LOG(Editor, Error, "Couldn't open scene '{}': {}", path, loaded.error().Message);
			m_toasts->Push(ToastKind::Error, "Couldn't open scene: " + loaded.error().Message);
			return;
		}

		// Undo steps point at the old scene's entities
		m_selection->Clear();
		m_undo->Clear();
	}
}
