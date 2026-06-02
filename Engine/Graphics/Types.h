#pragma once
#include "Core/Core.h"
#include "Core/Handle.h"
#include "Core/Math.h"
#include "Core/EnumFlags.h"
#include "Graphics/Format.h"
#include <string>
#include <string_view>
#include <array>
#include <vector>

namespace koi
{
	static constexpr u32 BACK_BUFFER_COUNT      = 3;
	static constexpr u32 FRAMES_IN_FLIGHT       = 3;
	static constexpr u32 MAX_RENDER_TARGETS     = 8;
	static constexpr u32 MAX_VERTEX_ATTRIBUTES  = 16;
	static constexpr u32 MAX_PUSH_CONSTANT_U32S = 32;

	template <typename T>
	using FrameArray = std::array<T, BACK_BUFFER_COUNT>;

	struct Texture;
	struct Buffer;
	struct Pipeline;
	struct Shader;

	using TextureHandle  = Handle<Texture>;
	using BufferHandle   = Handle<Buffer>;
	using PipelineHandle = Handle<Pipeline>;

	using DescriptorIndex = u32;
	inline constexpr DescriptorIndex INVALID_DESCRIPTOR = ~0u;

	enum class TextureUsage : u32
	{
		None         = 0,
		ShaderRead   = KOI_BIT(0),
		ShaderWrite  = KOI_BIT(1),
		RenderTarget = KOI_BIT(2),
		DepthStencil = KOI_BIT(3),
		Present      = KOI_BIT(4),
	};
	using TextureUsageFlag = EnumFlag<TextureUsage>;

	enum class BufferUsage : u32
	{
		None       = 0,
		Vertex     = KOI_BIT(0),
		Index      = KOI_BIT(1),
		Constant   = KOI_BIT(2),
		Storage    = KOI_BIT(3),
		Indirect   = KOI_BIT(4),
		CopySource = KOI_BIT(5),
		CopyDest   = KOI_BIT(6),
	};
	using BufferUsageFlag = EnumFlag<BufferUsage>;

	enum class MemoryUsage : u8
	{
		GpuOnly,     // DEFAULT heap
		CpuToGpu,    // UPLOAD heap (dynamic / per-frame data)
		GpuToCpu,    // READBACK heap
	};

	enum class CommandQueueType : u8 { Graphics, Compute, Copy };

	enum class ShaderType : u8
	{
		Vertex,
		Hull,
		Domain,
		Geometry,
		Pixel,
		Compute,

		MAX_COUNT
	};

	struct TextureDesc
	{
		u32 Width              = 1;
		u32 Height             = 1;
		u32 DepthOrArray       = 1;
		u32 MipLevels          = 1;
		u32 SampleCount        = 1;
		Format Format          = Format::R8G8B8A8_UNORM;
		TextureUsageFlag Usage = TextureUsage::ShaderRead;
		std::string_view DebugName;
	};

	struct BufferDesc
	{
		u64 Size              = 0;
		u32 Stride            = 0;     // Element stride for structured/vertex buffers
		BufferUsageFlag Usage = BufferUsage::None;
		MemoryUsage Memory    = MemoryUsage::GpuOnly;
		std::string_view DebugName;
	};

	struct VertexAttribute
	{
		std::string_view SemanticName = "";
		u32 SemanticIndex             = 0;
		Format Format                 = Format::UNKNOWN;
		u32 InputSlot                 = 0;
		u32 ByteOffset                = 0;
		bool PerInstance              = false;
	};

	struct IDeviceNotify
	{
		virtual ~IDeviceNotify() = default;
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};

	struct Vertex
	{
		math::float3 Position;
		math::float3 Normal;
		math::float4 Tangent;
		math::float2 UV;
	};

	struct ImageAsset
	{
		std::string Name;
		std::string Path;
		u32 MaterialIndex = 0;
		i32 Width         = -1;
		i32 Height        = -1;
		i32 Channels      = -1;
		std::vector<koi::byte> Pixels;
	};

	struct Material
	{
		enum class AlphaMode { Opaque, Mask, Blend };
		struct Constants
		{
			math::float4 BaseColorFactor  = math::float4(1.0f, 1.0f, 1.0, 1.0f);
			f32          MetallicFactor    = 1.0f;
			f32          RoughnessFactor   = 1.0f;
			f32          NormalScale       = 1.0f;
			f32          OcclusionStrength = 1.0f;
			math::float3 EmissiveFactor    = math::float3(0.0f, 0.0f, 0.0f);
			f32          AlphaCutoff       = 0.5f;
		};

		std::string Name;
		Constants MaterialConstants{};
		bool DoubleSided = false;
		AlphaMode Mode;

		u32 BaseColorTextureIdx         = u32(-1);
		u32 MetallicRoughnessTextureIdx = u32(-1);
		u32 NormalTextureIdx            = u32(-1);
		u32 OcclusionTextureIdx         = u32(-1);
		u32 EmissiveTextureIdx          = u32(-1);
	};

	struct SubMesh
    {
        u32 IndexOffset   = 0;
        u32 IndexCount    = 0;
        u32 VertexOffset  = 0;
        i32 MaterialIndex = -1;   // index into Scene::Materials, -1 = default
        math::AABB Bounds;
    };

	struct Mesh
	{
		std::string Name;
		std::vector<SubMesh> SubMeshes;
		std::vector<Vertex> Vertices;
		std::vector<u32> Indices;
		math::AABB Bounds;
		u32 VertexCount = 0;
		u32 IndexCount = 0;
	};

	struct ModelAsset
	{
		std::string SourcPath;
		std::vector<Mesh> Meshes;
		std::vector<Material> Materials;
		std::vector<ImageAsset> Images;
		math::AABB Bounds;
	};
}
KOI_ENABLE_ENUM_FLAGS(koi::TextureUsage);
KOI_ENABLE_ENUM_FLAGS(koi::BufferUsage);
