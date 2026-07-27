#pragma once
#include <concepts>
#include <memory>
#include <string>
#include <string_view>

namespace koi
{
	class GfxDevice;

	// Per-asset-type policy, and the only extension point the asset system has. Specializing this
	// for T is what makes T an asset type; an unspecialized T fails the AssetType concept below
	template<typename T>
	struct AssetTraits;

	// What a well-formed AssetTraits<T> has to provide. Constraining AssetCache/AssetRegistryFor on
	// this reports a missing or mis-typed member where the traits are written, rather than deep
	// inside the cache's instantiation.
	//
	//   template<>
	//   struct AssetTraits<GPUMesh>
	//   {
	//       static constexpr std::string_view Name = "Mesh";
	//       using Source = ModelAsset;            // discarded after upload
	//       static std::unique_ptr<Source> Load(const std::string& vfsPath);
	//       static GPUMesh Upload (GfxDevice& device, const Source& source);
	//       static void    Destroy(GfxDevice& device, GPUMesh& resource);
	//   };
	template<typename T>
	concept AssetType = requires(
		GfxDevice&                             device,
		const std::string&                     vfsPath,
		const typename AssetTraits<T>::Source& source,
		T&                                     resource)
	{
		// Human-readable name, used in load/failure logs
		{ AssetTraits<T>::Name }                      -> std::convertible_to<std::string_view>;

		// Reads the CPU-side intermediate off disk; returns null on failure
		{ AssetTraits<T>::Load(vfsPath) }             -> std::same_as<std::unique_ptr<typename AssetTraits<T>::Source>>;

		// Turns that intermediate into the live GPU resource the cache stores
		{ AssetTraits<T>::Upload(device, source) }    -> std::same_as<T>;

		// Releases whatever Upload created. Called once per asset by Release/ReleaseAll
		{ AssetTraits<T>::Destroy(device, resource) } -> std::same_as<void>;

		// TODO: Consider adding the Source type in the concept
	};
}
