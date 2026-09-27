#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include "Koi/Engine/Graphics/RHI/GpuProfiling.h"
#include <span>

namespace koi
{
	class GfxDevice;
	class CommandAllocator;

	class CommandList : NonCopyable
	{
	public:
		CommandList() = default;
		~CommandList() = default;

		[[nodiscard]] inline dx12::GraphicsCommandList* GetNative() const noexcept { return m_cmdList.Get(); }
		[[nodiscard]] inline dx12::CommandList* GetNativeBase() const noexcept { return m_cmdList.Get(); }
		[[nodiscard]] TracyD3D12Ctx GetProfilerCtx() const noexcept;
		[[nodiscard]] GpuTimers& GetGpuTimers() const noexcept;

		void Init(GfxDevice& device, CommandAllocator& allocator, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName = L"");

		void Begin(u32 frameSlot);
		void End();

		void TransitionBarrier(TextureHandle h, D3D12_RESOURCE_STATES newState);
		void TransitionBarrier(BufferHandle  h, D3D12_RESOURCE_STATES newState);
		void FlushBarriers();

		void ClearRenderTarget(TextureHandle rt, const f32 color[4]);
		void ClearDepthStencil(TextureHandle ds, f32 depth = 1.0f, u8 stencil = 0);

		// State binding
		void SetRenderTarget(TextureHandle colorRT, TextureHandle depthRT = {});
		void SetRenderTarget(std::span<TextureHandle> colorRTs, TextureHandle depthRT = {});
		void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth = 0.0f, f32 maxDepth = 1.0f);
		void SetScissor(u32 x, u32 y, u32 width, u32 height);
		void SetViewportAndScissor(u32 width, u32 height);
		void SetPipeline(PipelineHandle h);
		void SetVertexBuffer(u32 slot, BufferHandle h);
		void SetIndexBuffer(BufferHandle h);
		void SetRootConstants(const void* data, u32 count32BitValues, u32 destOffsetIn32BitValues = 0);

		// Draw
		void Draw(u32 vertexCount, u32 instanceCount = 1, u32 startVertex = 0, u32 startInstance = 0);
		void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 startIndex = 0, i32 vertexOffset = 0, u32 startInstance = 0);

		// Copy
		void CopyBuffer(BufferHandle dst, u64 dstOffset, BufferHandle src, u64 srcOffset, u64 size);

	private:
		GfxDevice*                          m_device    = nullptr;
		CommandAllocator*                   m_allocator = nullptr;
		D3D12_COMMAND_LIST_TYPE             m_type      = D3D12_COMMAND_LIST_TYPE_DIRECT;
		bool                                m_recording = false;
		ComPtr<dx12::GraphicsCommandList>   m_cmdList;
		std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
	};
}
