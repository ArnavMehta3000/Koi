#include "Koi/Editor/Assets/ImportQueue.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/TextureFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kProjectScheme = "project://";

		bool IsFinished(ImportStatus status)
		{
			return status == ImportStatus::Done || status == ImportStatus::Failed || status == ImportStatus::Cancelled;
		}

		// A virtual path when the file sits under a mounted root, so the project can move without breaking it
		std::string MakeSourceRef(const fs::path& source)
		{
			std::string ref = source.generic_string();
			VirtualFileSystem::Get().ForEachDiskRoot([&](std::string_view scheme, const fs::path& root)
			{
				std::error_code ec;
				const fs::path relative = fs::relative(source, root, ec);
				if (ec || relative.empty() || *relative.begin() == "..") return;
				if (ref.find("://") == std::string::npos)
				{
					ref = std::string(scheme) + "://" + relative.generic_string();
				}
			});
			return ref;
		}

		fs::path ResolveSourceRef(std::string_view ref)
		{
			if (ref.find("://") == std::string_view::npos) return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(ref.data()), ref.size()));
			return VirtualFileSystem::Get().TryResolveToDisk(ref).value_or(fs::path());
		}

		void RunBatch(const std::vector<std::shared_ptr<ImportItem>>& items)
		{
			for (const std::shared_ptr<ImportItem>& item : items)
			{
				if (item->Progress.IsCancelled())
				{
					item->Status.store(ImportStatus::Cancelled, std::memory_order_release);
					continue;
				}

				item->Status.store(ImportStatus::Running, std::memory_order_release);
				const auto start = std::chrono::steady_clock::now();

				std::string error;
				bool ok = false;
				try
				{
					if (item->Type == AssetFileType::Mesh)
					{
						ok = ImportModel(ModelImportRequest
						{
							.Source        = item->Source,
							.SourceRef     = item->SourceRef,
							.Folder        = item->OutputDisk.parent_path(),
							.VirtualFolder = item->OutputPath.substr(0, item->OutputPath.find_last_of('/')),
							.Name          = item->OutputDisk.stem().string(),
							.Id            = item->Id,
							.Settings      = item->Model,
						}, item->Progress, error);
					}
					else
					{
						ok = ImportTexture(TextureImportRequest
						{
							.Source    = item->Source,
							.SourceRef = item->SourceRef,
							.Output    = item->OutputDisk,
							.Id        = item->Id,
							.Settings  = item->Texture,
						}, item->Progress, error);
					}
				}
				catch (const std::exception& e)
				{
					error = std::string("the importer threw: ") + e.what();
				}

				item->Seconds = std::chrono::duration<f64>(std::chrono::steady_clock::now() - start).count();
				item->Error   = std::move(error);

				if (ok)
				{
					KOI_LOG(Import, Info, "Imported {} as {} in {:.1f} s ({})", item->Source.filename().string(), item->OutputPath, item->Seconds, item->Progress.Breakdown());
					item->Status.store(ImportStatus::Done, std::memory_order_release);
				}
				else if (item->Progress.IsCancelled())
				{
					KOI_LOG(Import, Info, "Stopped importing {}", item->Source.filename().string());
					item->Status.store(ImportStatus::Cancelled, std::memory_order_release);
				}
				else
				{
					KOI_LOG(Import, Error, "Couldn't import {}: {}", item->Source.filename().string(), item->Error);
					item->Status.store(ImportStatus::Failed, std::memory_order_release);
				}
			}
		}
	}

	ImportQueue::ImportQueue(AssetDatabase& database)
		: m_database(database)
	{
	}

	ImportQueue::~ImportQueue()
	{
		CancelAll();
		for (Batch& batch : m_batches)
		{
			if (batch.Work.valid()) batch.Work.wait();
		}
	}

	void ImportQueue::AddFiles(std::span<const fs::path> files)
	{
		ZoneScoped;

		u32 added = 0;
		const auto add = [&](const fs::path& file)
		{
			const bool model = IsModelSource(file);
			if (!model && !IsTextureSource(file))
			{
				KOI_LOG(Import, Info, "Skipping {}: only images and glTF models can be imported for now", file.filename().string());
				return;
			}

			const bool alreadyListed = std::ranges::any_of(m_pending, [&](const auto& item) { return item->Source == file; });
			if (alreadyListed) return;

			auto item     = std::make_shared<ImportItem>();
			item->Source  = file;
			item->Type    = model ? AssetFileType::Mesh : AssetFileType::Texture;
			item->Name    = file.stem().string();
			item->Texture = GuessTextureSettings(file);
			m_pending.push_back(std::move(item));
			++added;
		};

		for (const fs::path& path : files)
		{
			std::error_code ec;
			if (fs::is_directory(path, ec))
			{
				for (const fs::directory_entry& entry : fs::recursive_directory_iterator(path, ec))
				{
					if (entry.is_regular_file() && (IsTextureSource(entry.path()) || IsModelSource(entry.path()))) add(entry.path());
				}
			}
			else
			{
				add(path);
			}
		}

		if (added > 0)
		{
			m_wantsAttention = true;
			KOI_LOG(Import, Info, "{} file(s) ready to import. Check their settings in the Import panel", added);
		}
	}

	bool ImportQueue::Reimport(std::string_view assetPath)
	{
		const std::optional<AssetFile> file = AssetFile::Load(assetPath);
		if (!file) return false;

		// Both importers keep their settings and source in chunks with the same tags
		const bool isModel = file->Type() == AssetFileType::Mesh;
		const std::span<const byte> settingsBytes = file->ChunkBytes(TextureChunk::ImportSettings);
		const std::span<const byte> refBytes      = file->ChunkBytes(TextureChunk::SourcePath);
		const size_t settingsSize = isModel ? sizeof(ModelImportSettings) : sizeof(TextureImportSettings);
		if ((file->Type() != AssetFileType::Texture && !isModel) || settingsBytes.size() < settingsSize || refBytes.empty())
		{
			KOI_LOG(Import, Warn, "Can't reimport {}: it doesn't remember where it came from (a cooked copy, maybe?)", assetPath);
			return false;
		}

		const std::string ref(reinterpret_cast<const char*>(refBytes.data()), refBytes.size());
		const fs::path source = ResolveSourceRef(ref);
		std::error_code ec;
		if (source.empty() || !fs::exists(source, ec))
		{
			KOI_LOG(Import, Warn, "Can't reimport {}: its source {} isn't there anymore", assetPath, ref);
			return false;
		}

		m_wantsAttention = true;

		auto item        = std::make_shared<ImportItem>();
		item->Source     = source;
		item->Type       = file->Type();
		item->Name       = fs::path(assetPath).stem().string();
		if (isModel) std::memcpy(&item->Model,   settingsBytes.data(), sizeof(ModelImportSettings));
		else         std::memcpy(&item->Texture, settingsBytes.data(), sizeof(TextureImportSettings));
		item->SourceRef  = ref;
		item->OutputPath = std::string(assetPath);
		item->OutputDisk = VirtualFileSystem::Get().TryResolveToDisk(assetPath).value_or(fs::path());
		item->Id         = file->Id();  // same id, so everything pointing at it keeps working

		KOI_LOG(Import, Info, "Reimporting {} from {}", assetPath, ref);
		Start({ std::move(item) });
		return true;
	}

	void ImportQueue::StartPending()
	{
		ZoneScoped;
		if (m_pending.empty()) return;

		// Images and models each have their own destination
		struct Destination { std::string Folder; fs::path Disk; };
		const auto resolve = [](std::string folder) -> std::optional<Destination>
		{
			while (folder.ends_with('/')) folder.pop_back();
			if (!folder.starts_with(kProjectScheme))
			{
				KOI_LOG(Import, Error, "Imports have to land in a project folder, and '{}' isn't one", folder);
				return std::nullopt;
			}

			const fs::path disk = VirtualFileSystem::Get().TryResolveToDisk(folder).value_or(fs::path());
			if (disk.empty())
			{
				KOI_LOG(Import, Error, "Couldn't work out where {} is on disk", folder);
				return std::nullopt;
			}
			return Destination{ std::move(folder), disk };
		};

		const std::optional<Destination> images = resolve(m_folder);
		const std::optional<Destination> models = resolve(m_modelFolder);
		if (!images || !models) return;

		std::vector<std::string> taken;
		for (const std::shared_ptr<ImportItem>& item : m_pending)
		{
			// Two sources with the same name (a.png, a.jpg) would otherwise overwrite each other
			std::string name = item->Name.empty() ? item->Source.stem().string() : item->Name;
			for (int n = 2; std::ranges::contains(taken, name); ++n)
				name = item->Name + "_" + std::to_string(n);
			taken.push_back(name);

			// A model gets a folder of its own, for the materials and textures it brings along
			const bool isModel = item->Type == AssetFileType::Mesh;
			const Destination& destination = isModel ? *models : *images;
			const std::string fileName = name + std::string(AssetFile::ExtensionFor(item->Type));
			const std::string subdir   = isModel ? name + "/" : std::string();
			item->SourceRef  = MakeSourceRef(item->Source);
			item->OutputPath = destination.Folder + "/" + subdir + fileName;
			item->OutputDisk = destination.Disk / subdir / fileName;

			// Importing over an existing file keeps its id, so references to it survive
			const std::optional<AssetFileHeader> existing = fs::exists(item->OutputDisk) ? AssetFile::ReadHeader(item->OutputDisk) : std::nullopt;
			item->Id = existing ? UUID(existing->Id) : utils::NewUUID();
		}

		Start(std::move(m_pending));
		m_pending.clear();
	}

	void ImportQueue::Start(std::vector<std::shared_ptr<ImportItem>> items)
	{
		m_history.insert(m_history.end(), items.begin(), items.end());

		Batch batch{ .Items = std::move(items) };
		batch.Work = std::async(std::launch::async, [items = batch.Items] { RunBatch(items); });
		m_batches.push_back(std::move(batch));
	}

	void ImportQueue::CancelAll()
	{
		for (const std::shared_ptr<ImportItem>& item : m_history)
		{
			if (!IsFinished(item->Status.load(std::memory_order_acquire)))
				item->Progress.RequestCancel();
		}
	}

	void ImportQueue::ClearFinished()
	{
		std::erase_if(m_history, [](const auto& item) { return IsFinished(item->Status.load(std::memory_order_acquire)); });
	}

	void ImportQueue::Update()
	{
		const i64 running = std::ranges::count_if(m_history, [](const auto& item) { return !IsFinished(item->Status.load(std::memory_order_acquire)); });
		if (running > 0) FrameStats::Count("Imports running", running);

		bool finishedAny = false;
		for (auto it = m_batches.begin(); it != m_batches.end(); )
		{
			if (it->Work.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				++it;
				continue;
			}

			it->Work.get();
			for (const std::shared_ptr<ImportItem>& item : it->Items)
			{
				// The cache doesn't hot-swap yet, so whatever is already on screen keeps the old version
				AssetRegistry& assets = Engine::Get().GetSubsystem<AssetRegistry>();
				const AssetKey key(item->OutputPath);
				const bool resident = item->Type == AssetFileType::Mesh ? assets.Cache<GPUMesh>().IsResident(key) : assets.Cache<GPUTexture>().IsResident(key);
				if (item->Status == ImportStatus::Done && resident)
				{
					KOI_LOG(Import, Info, "{} is already loaded, so the new version shows up the next time the scene loads", item->OutputPath);
				}
			}

			it = m_batches.erase(it);
			finishedAny = true;
		}

		if (finishedAny && m_batches.empty())
			m_database.Refresh();  // picks up the new files and re-indexes their ids
	}
}
