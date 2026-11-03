#pragma once
#include "Koi/Engine/Core/EnumFlags.h"
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Core/Macros.h"
#include "Koi/Engine/Graphics/Format.h"
#include <array>
#include <string_view>

namespace koi
{
	static constexpr u32 FRAMES_IN_FLIGHT       = 3;  // also the swapchain buffer count: each frame waits on its back buffer's fence
	static constexpr u32 MAX_RENDER_TARGETS     = 8;  // D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT
	static constexpr u32 MAX_VERTEX_ATTRIBUTES  = 16;
	static constexpr u32 MAX_PUSH_CONSTANT_U32S = 32;

	template <typename T>
	using FrameArray = std::array<T, FRAMES_IN_FLIGHT>;

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

	enum class ShaderType
	{
		Unknown = -1,
		Vertex,
		Hull,
		Domain,
		Geometry,
		Pixel,
		Compute,

		MAX_COUNT
	};

	// Read from the shader's reflection data
	struct ShaderInfo
	{
		std::array<u32, 3> ThreadGroup{};  // [numthreads], compute only
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
		const char* SemanticName      = "";
		u32 SemanticIndex             = 0;
		Format Format                 = Format::UNKNOWN;
		u32 InputSlot                 = 0;
		u32 ByteOffset                = 0;
		bool PerInstance              = false;
	};

	// Where one mip of one array slice sits in a block of upload data. Also stored as-is in .ktex files
	struct TextureSubresourceLayout
	{
		u64 Offset   = 0;
		u32 RowPitch = 0;
		u32 NumRows  = 0;  // block rows for compressed formats
		u32 RowBytes = 0;  // the meaningful part of each row, the rest is padding
		u32 Reserved = 0;
	};
	static_assert(sizeof(TextureSubresourceLayout) == 24);
}
KOI_ENABLE_ENUM_FLAGS(koi::TextureUsage);
KOI_ENABLE_ENUM_FLAGS(koi::BufferUsage);
