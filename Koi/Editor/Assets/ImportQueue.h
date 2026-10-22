#pragma once
#include "Koi/Editor/Assets/ModelImporter.h"
#include "Koi/Editor/Assets/TextureImporter.h"
#include <functional>
#include <future>
#include <memory>
#include <utility>

namespace koi::editor
{
	class AssetDatabase;

	enum class ImportStatus : u8 { Waiting, Running, Done, Failed, Cancelled };

	struct ImportItem
	{
		fs::path              Source;
		AssetFileType         Type = AssetFileType::Texture;  // Mesh means a glTF model and everything it brings
		std::string           Name;                           // output file name, no extension
		TextureImportSettings Texture;
		ModelImportSettings   Model;

		// Filled in when the batch starts
		std::string           SourceRef;                      // the source as a virtual path when it lives under a mount
		std::string           OutputPath;                     // virtual; for a model, its .kmesh
		fs::path              OutputDisk;
		UUID                  Id;

		// The worker writes these; Error and Seconds only once Status says it's finished
		std::atomic<ImportStatus> Status{ ImportStatus::Waiting };
		ImportProgress            Progress;
		std::string               Error;
		f64                       Seconds = 0.0;
	};

	// Files waiting on their settings, then batches converting in the background. Ticked by the editor each frame
	class ImportQueue : public NonCopyable
	{
	public:
		explicit ImportQueue(AssetDatabase& database);
		~ImportQueue();

		// Cancels whatever is running and waits for it
		void OnShutdown();

		// Anything that isn't an image or a glTF is skipped with a log line saying why
		void AddFiles(std::span<const fs::path> files);

		// Rebuilds an existing .ktex or .kmesh from its remembered source and settings, straight away
		bool Reimport(std::string_view assetPath);

		[[nodiscard]] std::vector<std::shared_ptr<ImportItem>>& Pending() { return m_pending; }
		[[nodiscard]] std::span<const std::shared_ptr<ImportItem>> History() const { return m_history; }

		[[nodiscard]] std::string& DestinationFolder()      { return m_folder; }
		[[nodiscard]] std::string& ModelDestinationFolder() { return m_modelFolder; }

		void StartPending();
		void ClearPending() { m_pending.clear(); }
		void CancelAll();
		void ClearFinished();

		[[nodiscard]] bool IsBusy() const { return !m_batches.empty(); }

		// Runs when files are added or a reimport starts, so the Import panel can come forward
		void OnAttention(std::function<void()> callback) { m_onAttention = std::move(callback); }

		// Collects finished batches and refreshes the asset database once they're all in
		void Update();

	private:
		void Start(std::vector<std::shared_ptr<ImportItem>> items);

	private:
		struct Batch
		{
			std::vector<std::shared_ptr<ImportItem>> Items;
			std::future<void>                        Work;
		};

		AssetDatabase&                           m_database;
		std::string                              m_folder      = "project://Content/Textures";
		std::string                              m_modelFolder = "project://Content/Models";
		std::vector<std::shared_ptr<ImportItem>> m_pending;
		std::vector<std::shared_ptr<ImportItem>> m_history;
		std::vector<Batch>                       m_batches;
		std::function<void()>                    m_onAttention;
	};
}
