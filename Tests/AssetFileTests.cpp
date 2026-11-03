#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Assets/MeshFile.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Generated/Engine/EditorConfig.h"
#include <doctest/doctest.h>
#include <array>

using namespace koi;

namespace
{
	constexpr u32 kData   = utils::FourCC("DATA");
	constexpr u32 kSource = utils::FourCC("SRCE");

	AssetFile BuildTestAsset(const koi::UUID& id)
	{
		const std::array<u32, 3> data   = { 1, 2, 3 };
		const std::array<u32, 1> source = { 42 };

		AssetFileWriter writer(AssetFileType::Material, id);
		writer.AddChunkArray<u32>(kData, 1, data);
		writer.AddChunkArray<u32>(kSource, 1, source, ChunkFlags::EditorOnly);

		Result<AssetFile> file = AssetFile::FromBytes(writer.Build());
		REQUIRE(file);
		return std::move(*file);
	}
}

TEST_CASE("AssetFile write and read back")
{
	const koi::UUID id = utils::NewUUID();
	const AssetFile file = BuildTestAsset(id);

	CHECK(file.Type() == AssetFileType::Material);
	CHECK(file.Id() == id);
	CHECK(file.Chunks().size() == 2);

	const std::span<const u32> data = file.ChunkArray<u32>(kData);
	REQUIRE(data.size() == 3);
	CHECK(data[2] == 3);
}

TEST_CASE("Cooking strips EditorOnly chunks and keeps the rest")
{
	const koi::UUID id = utils::NewUUID();
	Result<AssetFile> cooked = AssetFile::FromBytes(BuildTestAsset(id).Cooked());
	REQUIRE(cooked);

	CHECK(cooked->Id() == id);
	CHECK(cooked->FindChunk(kData));
	CHECK_FALSE(cooked->FindChunk(kSource));
}

TEST_CASE("AssetFile rejects bytes that aren't an asset")
{
	CHECK_FALSE(AssetFile::FromBytes({}));
	CHECK_FALSE(AssetFile::FromBytes(std::vector<byte>(128, byte(0xAB))));
}

TEST_CASE("TypeForPath maps extensions")
{
	CHECK(AssetFile::TypeForPath("Box.kmesh")   == AssetFileType::Mesh);
	CHECK(AssetFile::TypeForPath("Wood.ktex")   == AssetFileType::Texture);
	CHECK(AssetFile::TypeForPath("Default.kmat") == AssetFileType::Material);
	CHECK(AssetFile::TypeForPath("Main.scene.toml") == AssetFileType::Unknown);
}

TEST_CASE("Engine Box.kmesh loads through the VFS")
{
	VirtualFileSystem::Get().Mount("engine", std::make_unique<FilesystemMount>(fs::path(KOI_ENGINE_DIR)));

	Result<AssetFile> file = AssetFile::Load("engine://Content/Meshes/Box.kmesh");
	REQUIRE_MESSAGE(file, (file ? "" : file.error().Message));
	CHECK(file->Type() == AssetFileType::Mesh);
	CHECK(file->Id() == utils::UUIDFromName("Koi.Engine.Mesh.Box"));

	Result<std::shared_ptr<const PreparedMesh>> mesh = PreparedMesh::Read(std::move(*file));
	REQUIRE(mesh);
	CHECK_FALSE((*mesh)->Vertices.empty());
	CHECK((*mesh)->Indices.size() % 3 == 0);
	CHECK_FALSE((*mesh)->Submeshes.empty());
}
