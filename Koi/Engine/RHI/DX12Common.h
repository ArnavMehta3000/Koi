#pragma once
#include "Koi/Engine/Core/Core.h"  // For the Win32 macro

#include "Koi/Engine/Core/Win32.h"
#include <wrl/client.h>

#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace koi
{
	namespace dx12
	{
		using Object              = ID3D12Object;
		using Device              = ID3D12Device8;
		using CommandList         = ID3D12CommandList;
		using GraphicsCommandList = ID3D12GraphicsCommandList6;
		using Resource            = ID3D12Resource2;
		using Heap                = ID3D12Heap1;
		using InfoQueue           = ID3D12InfoQueue1;
		using Fence               = ID3D12Fence1;
		using CommandQueue        = ID3D12CommandQueue;
		using CommandAllocator    = ID3D12CommandAllocator;
		using DescriptorHeap      = ID3D12DescriptorHeap;
		using PipelineState       = ID3D12PipelineState;
		using RootSignature       = ID3D12RootSignature;
	}

	namespace dxgi
	{
		using Factory   = IDXGIFactory7;
		using SwapChain = IDXGISwapChain4;
		using Adapter   = IDXGIAdapter4;
	}

	template <class T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	namespace internal
	{
		template <typename T>
		struct IsComPtr : std::false_type {};

		template<typename U>
		struct IsComPtr<ComPtr<U>> : std::true_type {};

		template<typename T>
		concept ComPtrType = IsComPtr<std::remove_cvref_t<T>>::value;

		template <typename T>
		concept RawComPtrType = std::is_pointer_v<T>
			&& std::is_class_v<std::remove_pointer_t<T>>
			&& requires(T p) { p->Release(); };

		[[noreturn]] void ThrowHResult(HRESULT hr, const char* expr, const std::source_location& where);

		inline void CheckHResult(HRESULT hr, const char* expr, const std::source_location& where = std::source_location::current())
		{
			if (FAILED(hr)) [[unlikely]]
			{
				ThrowHResult(hr, expr, where);
			}
		}
	}

	class GfxError : public std::runtime_error
	{
	public:
		GfxError(HRESULT hr, const std::string& message) : std::runtime_error(message), m_result(hr) {}

		[[nodiscard]] HRESULT GetResult() const noexcept { return m_result; }

	private:
		HRESULT m_result;
	};

	#define DX_CHECK(expr) ::koi::internal::CheckHResult((expr), #expr)
}
