#pragma once
#include "Koi/Engine/Diagnostics/FrameStats.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include <array>
#include <span>
#include <vector>

namespace koi
{
	class CommandList;

	class GpuTimers : NonCopyable
	{
	public:
		void Init(dx12::Device* device, dx12::CommandQueue* queue);
		void Shutdown();

		void BeginFrame(u64 completedFence);
		void EndFrame(u64 fenceValue);

		[[nodiscard]] u32 BeginScope(dx12::GraphicsCommandList* cmd, std::string_view name);
		void EndScope(dx12::GraphicsCommandList* cmd, u32 scope);
		void Resolve(dx12::GraphicsCommandList* cmd);

		[[nodiscard]] inline std::span<const FrameStatSample> GetLastFrame() const noexcept { return m_last; }

		static constexpr u32 kInvalidScope = ~0u;

	private:
		static constexpr u32 kMaxScopes      = 32;
		static constexpr u32 kQueriesPerSlot = kMaxScopes * 2;

		struct Slot
		{
			std::vector<std::string_view> Names;
			u64                           Fence    = 0;
			bool                          Resolved = false;
		};

		void ReadBack(const Slot& slot, u32 slotIndex);

	private:
		ComPtr<ID3D12QueryHeap>              m_heap;
		ComPtr<dx12::Resource>               m_readback;
		std::array<Slot, FRAMES_IN_FLIGHT>   m_slots;
		std::vector<FrameStatSample>         m_last;
		f64                                  m_ticksToSeconds = 0.0;
		u32                                  m_slot           = 0;
		u32                                  m_depth          = 0;
	};

	class GpuStatScope : NonCopyable
	{
	public:
		GpuStatScope(CommandList& cmd, std::string_view name);
		~GpuStatScope();

	private:
		CommandList& m_cmd;
		u32          m_scope = GpuTimers::kInvalidScope;
	};
}
