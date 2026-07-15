#pragma once
#include "Koi/Engine/Core/Core.h"  // For the Win32 macro

#include "Koi/Engine/Platform/Win32.h"
#include <wrl/client.h>

#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <concepts>

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
	}

	// Result-check helper. Logs + asserts on failure.
	void ThrowOnHRFail(HRESULT hr, const char* expr, const char* file, int line);

#if KOI_DEBUG
	#define DX_CHECK(expr) ::koi::ThrowOnHRFail((expr), #expr, (KOI_FILENAME).data(), __LINE__)
#else
	#define DX_CHECK(expr) (void)(expr)
#endif
}
