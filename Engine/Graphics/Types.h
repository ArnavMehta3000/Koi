#pragma once
#include "Core/Core.h"
#include "Core/Handle.h"
#include "Core/Math.h"
#include "Graphics/Format.h"
#include <Kirin/Common/Macros.h>
#include <Kirin/Core/Enum.h>
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
	using ShaderHandle   = Handle<Shader>;

	using DescriptorIndex = u32;
	inline constexpr DescriptorIndex INVALID_DESCRIPTOR = ~0u;

	enum class TextureUsage : u32
	{
		None         = 0,
		ShaderRead   = KRN_BIT(0),
		ShaderWrite  = KRN_BIT(1),
		RenderTarget = KRN_BIT(2),
		DepthStencil = KRN_BIT(3),
		Present      = KRN_BIT(4),
	};
	using TextureUsageFlag = krn::EnumFlag<TextureUsage>;

	enum class BufferUsage : u32
	{
		None       = 0,
		Vertex     = KRN_BIT(0),
		Index      = KRN_BIT(1),
		Constant   = KRN_BIT(2),
		Storage    = KRN_BIT(3),
		Indirect   = KRN_BIT(4),
		CopySource = KRN_BIT(5),
		CopyDest   = KRN_BIT(6),
	};
	using BufferUsageFlag = krn::EnumFlag<BufferUsage>;

	enum class MemoryUsage : u8
	{
		GpuOnly,     // DEFAULT heap
		CpuToGpu,    // UPLOAD heap (dynamic / per-frame data)
		GpuToCpu,    // READBACK heap
	};

	enum class CommandQueueType : u8 { Graphics, Compute, Copy };

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

	struct Material
	{
		std::string Name;
		math::float4 BaseColorFactor;
		i32 BaseColorTexture;
		i32 NormalTexture;
		i32 MetallicRoughnessTexture;
		f32 MetallicFactor = 1.f;
		f32 RoughnessFactor = 1.f;
	};

	struct SubMesh
	{
		std::vector<Vertex> Vertices;
		std::vector<u32> Indices;
		u32 MaterialIndex;
		math::AABB Bounds;
	};

	struct Mesh
	{
		std::string Name;
		std::vector<SubMesh> SubMeshes;
		math::AABB Bounds;
	};

	struct ImageAsset
	{
		std::string Name;
		std::string Path;
		u32 Width = 0;
		u32 Height = 0;
		u32 Channels = 0;
		std::vector<koi::byte> Pixels;
	};

	struct ModelAsset
	{
		std::string Name;
		std::vector<Mesh> Meshes;
		std::vector<Material> Materials;
		std::vector<ImageAsset> Images;
		math::AABB Bounds;
	};
}
KRN_ENABLE_ENUM_FLAGS(koi::TextureUsage);
KRN_ENABLE_ENUM_FLAGS(koi::BufferUsage);
