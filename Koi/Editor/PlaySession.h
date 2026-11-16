#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Core/Types.h"
#include <toml++/toml.hpp>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace koi::editor
{
	class EditorContext;
	class Selection;
	class Toasts;

	// Play-in-editor: snapshots the world on Play and restores it on Stop
	class PlaySession : public NonCopyable
	{
	public:
		explicit PlaySession(EditorContext* ctx);

		void Play();
		void TogglePause();
		void Stop();
		void TogglePlay();  // Play from Edit, Stop otherwise

		// Logs and toasts a failure from game code; the caller decides whether to Stop
		void ReportFailure(const std::exception& e);

		// Callbacks fired before game's begin play and after world restore
		inline void OnStarted(std::function<void()> callback) { m_onStarted.push_back(std::move(callback)); }
		inline void OnStopped(std::function<void()> callback) { m_onStopped.push_back(std::move(callback)); }

	private:
		EditorContext*                     m_ctx       = nullptr;
		Selection*                         m_selection = nullptr;
		Toasts*                            m_toasts    = nullptr;
		toml::table                        m_snapshot;         // the world as it was when Play was pressed
		std::string                        m_snapshotPath;
		std::vector<MeshHandle>            m_pinnedMeshes;     // keep the snapshot's assets resident across Stop
		std::vector<MaterialHandle>        m_pinnedMaterials;
		std::vector<std::function<void()>> m_onStarted;
		std::vector<std::function<void()>> m_onStopped;
	};
}
