#pragma once
#include "Core/Core.h"
#include "Core/Handle.h"
#include "Graphics/Format.h"
#include <Kirin/Common/Macros.h>
#include <Kirin/Core/Enum.h>
#include <string_view>
#include <array>

namespace koi
{
	static constexpr u32 BACK_BUFFER_COUNT = 3;

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

	enum class MemoryUsage : u8
	{
		GpuOnly,     // DEFAULT heap
		CpuToGpu,    // UPLOAD heap (dynamic / per-frame data)
		GpuToCpu,    // READBACK heap
	};

	enum class CommandQueueType : u8 { Graphics, Compute, Copy };

	struct TextureDesc
	{
		u32 Width                         = 1;
		u32 Height                        = 1;
		u32 DepthOrArray                  = 1;
		u32 MipLevels                     = 1;
		u32 SampleCount                   = 1;
		Format Format                     = Format::R8G8B8A8_UNORM;
		krn::EnumFlag<TextureUsage> Usage = TextureUsage::ShaderRead;
		std::string_view DebugName;
	};

	struct BufferDesc
	{
		u64 Size                         = 0;
		u32 Stride                       = 0;     // Element stride for structured/vertex buffers
		krn::EnumFlag<BufferUsage> Usage = BufferUsage::None;
		MemoryUsage Memory               = MemoryUsage::GpuOnly;
		std::string_view DebugName;
	};

	struct IDeviceNotify
	{
		virtual ~IDeviceNotify() = default;
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};
}
KRN_ENABLE_ENUM_FLAGS(koi::TextureUsage);
KRN_ENABLE_ENUM_FLAGS(koi::BufferUsage);
