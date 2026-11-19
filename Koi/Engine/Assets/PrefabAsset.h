#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include <memory>
#include <string>
#include <string_view>

namespace toml { inline namespace v3 { class table; } }

namespace koi
{
	// Loaded once per .prefab.toml
	class IPrefabTemplate
	{
	public:
		virtual ~IPrefabTemplate() = default;

		[[nodiscard]] virtual std::string_view Class() const = 0;
		virtual void Load(const toml::table& file, std::string_view path) = 0;
	};

	struct PrefabTemplate
	{
		std::unique_ptr<IPrefabTemplate> Data;
	};

	struct PrefabSource
	{
		std::unique_ptr<toml::table> File;
		std::string                  Path;

		PrefabSource();
		~PrefabSource();
	};

	template<>
	struct AssetTraits<PrefabTemplate>
	{
		static constexpr std::string_view Name = "Prefab";

		using Source = PrefabSource;

		static Result<std::unique_ptr<Source>> Load(const std::string& vfsPath);
		static PrefabTemplate Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, PrefabTemplate& resource);
	};

	using PrefabHandle = AssetHandle<PrefabTemplate>;
}
