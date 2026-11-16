#include "Koi/Editor/PlaySession.h"

#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Editor/Toasts.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Diagnostics/CrashHandler.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi::editor
{
	PlaySession::PlaySession(EditorContext* ctx)
		: m_ctx(ctx)
		, m_selection(ctx->Get<Selection>())
		, m_toasts(ctx->Get<Toasts>())
	{
	}

	void PlaySession::TogglePlay()
	{
		if (Engine::Get().GetPlayState() == PlayState::Edit)
		{
			Play();
		}
		else
		{
			Stop();
		}
	}

	void PlaySession::Play()
	{
		ZoneScoped;
		for (const std::function<void()>& callback : m_onStarted)
		{
			callback();
		}

		World& world                    = m_ctx->GetGame()->GetWorld();
		Engine& engine                  = Engine::Get();
		GfxDevice& device               = engine.GetSubsystem<Renderer>().GetDevice();
		AssetRegistry& assets           = engine.GetSubsystem<AssetRegistry>();
		AssetCache<GPUMesh>& meshes     = assets.Cache<GPUMesh>();
		AssetCache<Material>& materials = assets.Cache<Material>();

		m_snapshot     = world.SaveToTable();
		m_snapshotPath = world.GetSourcePath();

		// Hold a reference to everything the snapshot names. Stop tears the world down
		// before rebuilding it, and without this the last reference to each asset would
		// drop to zero and the same files would be re-read from disk a moment later
		world.GetECS().Each<const MeshRef>([&](const MeshRef& mesh)
		{
			const AssetKey key = meshes.GetKey(mesh.Handle);
			if (key) m_pinnedMeshes.push_back(meshes.Acquire(device, key));  // Acquire increases the internal ref-count
		});

		world.GetECS().Each<const MaterialRef>([&](const MaterialRef& material)
		{
			const AssetKey key = materials.GetKey(material.Handle);
			if (key) m_pinnedMaterials.push_back(materials.Acquire(device, key));  // Acquire increases the internal ref-count
		});

		engine.SetPlayState(PlayState::Playing);  // Fires OnBeginPlay, spawns discarded
	}

	void PlaySession::TogglePause()
	{
		ZoneScoped;
		Engine& engine = Engine::Get();
		switch (engine.GetPlayState())
		{
			case PlayState::Playing: engine.SetPlayState(PlayState::Paused);  break;
			case PlayState::Paused:  engine.SetPlayState(PlayState::Playing); break;
			case PlayState::Edit:                                             break;
		}
	}

	void PlaySession::ReportFailure(const std::exception& e)
	{
		KOI_LOG(Editor, Error, "Play stopped: {}\n{}", e.what(), CrashHandler::Format(CrashHandler::TraceOf(e)));
		m_toasts->Push(ToastKind::Error, std::string("Play stopped: ") + e.what());
	}

	void PlaySession::Stop()
	{
		ZoneScoped;

		Engine& engine = Engine::Get();
		if (engine.GetPlayState() == PlayState::Edit) return;

		// Leave play mode first: OnEndPlay still needs the world it ran against
		engine.SetPlayState(PlayState::Edit);

		// The rebuild below recycles entity ids, so a stale selection would resolve to whatever entity happens to land in that slot
		m_selection->Clear();

		World& world = m_ctx->GetGame()->GetWorld();
		world.Load(m_snapshot, m_snapshotPath);  // a scene loaded during play doesn't leave its path behind

		// The rebuilt world took its own references, so the play-time pins can go
		GfxDevice& device     = engine.GetSubsystem<Renderer>().GetDevice();
		AssetRegistry& assets = engine.GetSubsystem<AssetRegistry>();
		for (const MeshHandle handle : m_pinnedMeshes)        { assets.Cache<GPUMesh>().Release(device, handle);  }
		for (const MaterialHandle handle : m_pinnedMaterials) { assets.Cache<Material>().Release(device, handle); }

		m_pinnedMeshes.clear();
		m_pinnedMaterials.clear();
		m_snapshot = toml::table{};
		m_snapshotPath.clear();

		for (const std::function<void()>& callback : m_onStopped)
		{
			callback();
		}
		KOI_LOG(Editor, Debug, "Stop: world restored from snapshot, pinned assets released");
	}
}
