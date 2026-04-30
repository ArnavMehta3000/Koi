#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Core/Logger.h"
#include "Core/StringUtils.h"

#if KOI_DEBUG
	#include <dxgidebug.h>
#endif

namespace koi
{
	CD3DX12FeatureSupport g_featureSupport;

	static D3D12_RESOURCE_FLAGS ToD3DFlags(krn::EnumFlag<TextureUsage> u)
	{
		D3D12_RESOURCE_FLAGS f = D3D12_RESOURCE_FLAG_NONE;
		if (u.Has(TextureUsage::RenderTarget)) f |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (u.Has(TextureUsage::DepthStencil)) f |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (u.Has(TextureUsage::ShaderWrite))  f |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (u.Has(TextureUsage::DepthStencil) && !u.Has(TextureUsage::ShaderRead))
			f |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		return f;
	}

	static void ReportLiveObjects()
	{
#if KOI_DEBUG
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
		{
			debug->ReportLiveObjects(
				DXGI_DEBUG_ALL,
				DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)
			);
		}
#endif
	}

	// From DX12Common.h
	void ThrowOnHRFail(HRESULT hr, const char* expr, const char* file, int line)
	{
		if (FAILED(hr))
		{
			KOI_LOG_ERROR("D3D12 call failed (0x{:08X}): {} at {}:{}",
				static_cast<u32>(hr), expr, file, line);
			KOI_ASSERT_MSG(false, "D3D12 call failed");
		}
	}

	GfxDevice::GfxDevice(const GfxDevice::Desc& desc)
		: m_desc(desc)
	{
		CreateFactory(desc.EnableDebugLayer);
		PickAdapter();
		CreateDevice();
		CreateQueues();
		CreateFences();
		CreateHeaps();
		CreateRootSignature();
	}

	GfxDevice::~GfxDevice()
	{
		KOI_LOG_TRACE("Destroying D3D12 device");

		WaitForIdle();
		FlushDeferredDeletions(~0ull);

		if (m_fenceEvent)
		{
			::CloseHandle(m_fenceEvent);
			m_fenceEvent = nullptr;
		}
		m_frameFence    = nullptr;
		m_graphicsQueue = nullptr;
		m_rootSignature = nullptr;
		m_adapter       = nullptr;
		m_dxgiFactory   = nullptr;
		m_device        = nullptr;

		m_rtvHeap.Reset();
		m_dsvHeap.Reset();
		m_resourceHeap.Reset();
		m_samplerHeap.Reset();

		ReportLiveObjects();

		// [NOTE]: Not clearing resource pool here
		KOI_LOG_TRACE("Finished destroying D3D12 device");
	}

	u64 GfxDevice::GetCompletedFenceValue() const noexcept
	{
		return m_frameFence ? m_frameFence->GetCompletedValue() : 0;
	}

	TextureHandle GfxDevice::RegisterSwapChainImage(ComPtr<dx12::Resource> resource, const TextureDesc& desc)
	{
		Texture tex
		{
			.Resource         = resource,
			.Desc             = desc,
			.CurrentState     = D3D12_RESOURCE_STATE_PRESENT,
			.IsSwapChainImage = true
		};
		return m_textures.Emplace(tex);
	}

	TextureHandle GfxDevice::CreateTexture(const TextureDesc& desc)
	{
		Texture tex{ .Desc = desc, .IsSwapChainImage = false };

		auto rd = CD3DX12_RESOURCE_DESC::Tex2D(
			ToDXGIFormat(desc.Format),
			desc.Width, desc.Height,
			static_cast<u16>(desc.DepthOrArray),
			static_cast<u16>(desc.MipLevels),
			desc.SampleCount, 0U,  // [NOTE]: Sample quality is 0
			ToD3DFlags(desc.Usage),
			D3D12_TEXTURE_LAYOUT_UNKNOWN
		);

		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		DX_CHECK(m_device->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &rd,
			initialState, nullptr, IID_PPV_ARGS(&tex.Resource)));

		tex.CurrentState = initialState;
		if (!desc.DebugName.empty())
		{
			const std::wstring wname = StringUtils::ToWide(desc.DebugName);
			tex.Resource->SetName(wname.c_str());
		}

		KOI_LOG_TRACE("Created texture: {}", desc.DebugName.empty() ? "<unnamed>" : desc.DebugName);

		return m_textures.Emplace(std::move(tex));
	}

	BufferHandle GfxDevice::CreateBuffer(const BufferDesc& desc)
	{
		Buffer buf{ .Desc = desc };

		D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		switch (desc.Memory)
		{
		case MemoryUsage::CpuToGpu:
			heapType     = D3D12_HEAP_TYPE_UPLOAD;
			initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			break;
		case MemoryUsage::GpuToCpu:
			heapType     = D3D12_HEAP_TYPE_READBACK;
			initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		default: break;
		}

		CD3DX12_HEAP_PROPERTIES heap(heapType);
		CD3DX12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(desc.Size);

		if (desc.Usage.Has(BufferUsage::Storage))
			rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		DX_CHECK(m_device->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &rd,
			initialState, nullptr, IID_PPV_ARGS(&buf.Resource)));

		buf.CurrentState = initialState;
		buf.GPUAddress   = buf.Resource->GetGPUVirtualAddress();

		if (desc.Memory == MemoryUsage::CpuToGpu)
		{
			CD3DX12_RANGE readRange(0, 0);
			DX_CHECK(buf.Resource->Map(0, &readRange, &buf.MappedPtr));
		}

		if (!desc.DebugName.empty())
		{
			const std::wstring wname = StringUtils::ToWide(desc.DebugName);
			buf.Resource->SetName(wname.c_str());
		}

		KOI_LOG_TRACE("Created buffer: {}", desc.DebugName.empty() ? "<unnamed>" : desc.DebugName);

		return m_buffers.Emplace(std::move(buf));
	}

	void GfxDevice::Destroy(TextureHandle h)
	{
		if (m_textures.IsValid(h))
		{
			PendingDeletion d;
			d.Type       = PendingDeletion::ResourceType::Texture;
			d.FenceValue = m_nextFenceValue;
			d.Texture    = h;

			m_deletionQueue.push(d);
			KOI_LOG_TRACE("Marked texture for deletion (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		}
		else
		{
			KOI_LOG_WARN("Trying to mark invalid texture for deletion (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		}
	}

	void GfxDevice::Destroy(BufferHandle h)
	{
		if (m_buffers.IsValid(h))
		{
			PendingDeletion d;
			d.Type       = PendingDeletion::ResourceType::Buffer;
			d.FenceValue = m_nextFenceValue;
			d.Buffer     = h;

			m_deletionQueue.push(d);
			KOI_LOG_TRACE("Marked buffer for deletion: {}", h.GetId());
		}
		else
		{
			KOI_LOG_WARN("Trying to mark invalid buffer for deletion (Hanlde: {})", h.GetId());
		}
	}

	u64 GfxDevice::SignalFrame()
	{
		/// DevNote: This function tells the GPU to write the current counter value into the fence
		/// when it finishes all the work so far, then increments the counter.
		/// It also flushes the defferred deltion queue

		const u64 value = m_nextFenceValue++;
		DX_CHECK(m_graphicsQueue->Signal(m_frameFence.Get(), value));
		++m_frameIndex;
		FlushDeferredDeletions(m_frameFence->GetCompletedValue());
		return value;
	}

	void GfxDevice::WaitForFence(u64 fenceValue)
	{
		/// DevNote: This function checks if the GPU has already passed that (signalled) value
		/// If not, then it uses `SetEventOnCompletion` + `WaitForSingleObject` to block the CPU until it has
		/// This is the DX12 equivalent of DX11' `Flush()` stall, except I have the control here

		if (m_frameFence->GetCompletedValue() >= fenceValue) return;
		DX_CHECK(m_frameFence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		::WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	void GfxDevice::WaitForIdle()
	{
		///DevNote: This is the nuclear option, it signal a new value and immediately waits for it
		/// draining the entire GPU pipeline. Used before resize and shutdown
		
		const u64 value = m_nextFenceValue++;
		DX_CHECK(m_graphicsQueue->Signal(m_frameFence.Get(), value));
		WaitForFence(value);
	}

	void GfxDevice::FlushPendingDeletions()
	{
		FlushDeferredDeletions(~0ull);
	}

	void GfxDevice::Destroy(PipelineHandle h)
	{
		if (m_pipelines.IsValid(h))
		{
			PendingDeletion d;
			d.Type       = PendingDeletion::ResourceType::Pipeline;
			d.FenceValue = m_nextFenceValue;
			d.Pipeline   = h;
			m_deletionQueue.push(d);
			KOI_LOG_TRACE("Marked pipeline for deletion (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		}
		else
		{
			KOI_LOG_WARN("Trying to mark invalid pipeline for deletion (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		}
	}

	PipelineHandle GfxDevice::CreatePipeline(const PipelineDesc& desc)
	{
		KOI_ASSERT_MSG(!desc.VertexShader.empty(), "PipelineDesc missing vertex shader");
		KOI_ASSERT_MSG(!desc.PixelShader.empty(),  "PipelineDesc missing pixel shader");
		KOI_ASSERT_MSG(desc.RenderTargetCount > 0 || desc.DepthFormat != Format::UNKNOWN,
			"PipelineDesc has neither render targets nor a depth target");

		// Vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputElements[MAX_VERTEX_ATTRIBUTES];
		u32 numElements = 0;
		for (const auto& attr : desc.VertexLayout)
		{
			KOI_ASSERT_MSG(numElements < MAX_VERTEX_ATTRIBUTES, "Too many vertex attributes");
			inputElements[numElements++] =
			{
				.SemanticName         = attr.SemanticName,
				.SemanticIndex        = attr.SemanticIndex,
				.Format               = ToDXGIFormat(attr.Format),
				.InputSlot            = attr.InputSlot,
				.AlignedByteOffset    = attr.ByteOffset,
				.InputSlotClass       = attr.PerInstance
					? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
					: D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = attr.PerInstance ? 1u : 0u,
			};
		}

		// Rasterizer
		D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rasterDesc.FillMode              = desc.Rasterizer.FillMode;
		rasterDesc.CullMode              = desc.Rasterizer.CullMode;
		rasterDesc.FrontCounterClockwise = desc.Rasterizer.FrontCounterClockwise;
		rasterDesc.DepthClipEnable       = desc.Rasterizer.DepthClipEnable;
		rasterDesc.ConservativeRaster    = desc.Rasterizer.ConservativeRaster
			? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON
			: D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		rasterDesc.DepthBias             = desc.Rasterizer.DepthBias;
		rasterDesc.DepthBiasClamp        = desc.Rasterizer.DepthBiasClamp;
		rasterDesc.SlopeScaledDepthBias  = desc.Rasterizer.SlopeScaledDepthBias;

		// Blend state
		D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		for (u32 i = 0; i < desc.RenderTargetCount; i++)
		{
			const BlendTargetDesc& src    = desc.BlendTargets[i];
			auto& dst                     = blendDesc.RenderTarget[i];
			dst.BlendEnable               = src.BlendEnable;
			dst.SrcBlend                  = src.SrcBlend;
			dst.DestBlend                 = src.DstBlend;
			dst.BlendOp                   = src.BlendOp;
			dst.SrcBlendAlpha             = src.SrcBlendAlpha;
			dst.DestBlendAlpha            = src.DstBlendAlpha;
			dst.BlendOpAlpha              = src.BlendOpAlpha;
			dst.RenderTargetWriteMask     = src.WriteMask;
		}

		// Depth-stencil
		D3D12_DEPTH_STENCIL_DESC depthDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		depthDesc.DepthEnable              = desc.DepthStencil.DepthEnable;
		depthDesc.DepthWriteMask           = desc.DepthStencil.DepthWrite
			? D3D12_DEPTH_WRITE_MASK_ALL
			: D3D12_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc                = desc.DepthStencil.DepthFunc;
		depthDesc.StencilEnable            = desc.DepthStencil.StencilEnable;

		// RT formats
		DXGI_FORMAT rtFormats[MAX_RENDER_TARGETS] = {};
		for (u32 i = 0; i < desc.RenderTargetCount; i++)
			rtFormats[i] = ToDXGIFormat(desc.RenderTargetFormats[i]);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature                     = m_rootSignature.Get();
		psoDesc.VS                                 = { desc.VertexShader.data(), desc.VertexShader.size() };
		psoDesc.PS                                 = { desc.PixelShader.data(),  desc.PixelShader.size()  };
		if (!desc.GeometryShader.empty()) psoDesc.GS = { desc.GeometryShader.data(), desc.GeometryShader.size() };
		if (!desc.HullShader.empty())     psoDesc.HS = { desc.HullShader.data(),     desc.HullShader.size()     };
		if (!desc.DomainShader.empty())   psoDesc.DS = { desc.DomainShader.data(),   desc.DomainShader.size()   };
		psoDesc.InputLayout                        = { inputElements, numElements };
		psoDesc.RasterizerState                    = rasterDesc;
		psoDesc.BlendState                         = blendDesc;
		psoDesc.DepthStencilState                  = depthDesc;
		psoDesc.PrimitiveTopologyType              = desc.TopologyType;
		psoDesc.NumRenderTargets                   = desc.RenderTargetCount;
		for (u32 i = 0; i < desc.RenderTargetCount; i++)
			psoDesc.RTVFormats[i] = rtFormats[i];
		psoDesc.DSVFormat                          = ToDXGIFormat(desc.DepthFormat);
		psoDesc.SampleDesc                         = { desc.SampleCount, 0 };
		psoDesc.SampleMask                         = UINT_MAX;

		Pipeline pipeline;
		pipeline.Topology = desc.Topology;

		DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline.Pso)));

		if (!desc.DebugName.empty())
		{
			const std::wstring wname = StringUtils::ToWide(std::string(desc.DebugName));
			pipeline.Pso->SetName(wname.c_str());
		}

		KOI_LOG_TRACE("Created pipeline: {}", desc.DebugName.empty() ? "<unnamed>" : desc.DebugName);
		return m_pipelines.Emplace(std::move(pipeline));
	}

	void GfxDevice::UploadBufferData(BufferHandle dst, const void* data, u64 size)
	{
		BufferHandle staging = CreateBuffer(BufferDesc
		{
			.Size      = size,
			.Usage     = BufferUsage::CopySource,
			.Memory    = MemoryUsage::CpuToGpu,
			.DebugName = "Upload Staging",
		});

		Buffer* stagingBuf = m_buffers.Get(staging);
		KOI_ASSERT_MSG(stagingBuf && stagingBuf->MappedPtr, "Staging buffer mapping failed");
		std::memcpy(stagingBuf->MappedPtr, data, size);

		Buffer* dstBuf = m_buffers.Get(dst);
		KOI_ASSERT_MSG(dstBuf, "Invalid destination buffer");

		ComPtr<dx12::CommandAllocator> allocator;
		DX_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

		ComPtr<dx12::GraphicsCommandList> cmdList;
		DX_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList)));

		if (dstBuf->CurrentState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				dstBuf->Resource.Get(), dstBuf->CurrentState, D3D12_RESOURCE_STATE_COPY_DEST);
			cmdList->ResourceBarrier(1, &barrier);
		}

		cmdList->CopyBufferRegion(dstBuf->Resource.Get(), 0, stagingBuf->Resource.Get(), 0, size);

		{
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				dstBuf->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
			cmdList->ResourceBarrier(1, &barrier);
			dstBuf->CurrentState = D3D12_RESOURCE_STATE_COMMON;
		}

		DX_CHECK(cmdList->Close());

		dx12::CommandList* lists[] = { cmdList.Get() };
		m_graphicsQueue->ExecuteCommandLists(1, lists);
		WaitForIdle();

		Destroy(staging);
		FlushPendingDeletions();
	}

	void GfxDevice::CreateRootSignature()
	{
		KOI_LOG_TRACE("Creating bindless root signature");

		const D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion = g_featureSupport.HighestRootSignatureVersion();

		CD3DX12_ROOT_PARAMETER1 rootParam;
		rootParam.InitAsConstants(MAX_PUSH_CONSTANT_U32S, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
		desc.Init_1_1(
			1, &rootParam,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  |
			D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
		);

		ComPtr<ID3DBlob> blob, error;
		HRESULT hr = ::D3DX12SerializeVersionedRootSignature(&desc, rootSignatureVersion, &blob, &error); 
		if (FAILED(hr))
		{
			if (error)
			{
				KOI_LOG_ERROR("Failed to serialize bindless root signature: {}",
					static_cast<const char*>(error->GetBufferPointer()));
			}
			DX_CHECK(hr);
		}

		DX_CHECK(m_device->CreateRootSignature(
			0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature)));

		m_rootSignature->SetName(L"Bindless Root Signature");
	}

	void GfxDevice::HandleDeviceLost()
	{
		KOI_LOG_CRITICAL("D3D12 device lost. Attempting recovery...");

		if (m_notify) m_notify->OnDeviceLost();

		// Drop all GPU resources and rebuild.
		FlushDeferredDeletions(~0ull);
		m_textures.Clear();
		m_buffers.Clear();
		m_pipelines.Clear();

		m_rootSignature = nullptr;
		m_graphicsQueue.Reset();
		m_frameFence.Reset();
		m_device.Reset();
		m_adapter.Reset();
		m_dxgiFactory.Reset();

		CreateFactory(m_desc.EnableDebugLayer);
		PickAdapter();
		CreateDevice();
		CreateQueues();
		CreateFences();
		CreateHeaps();
		CreateRootSignature();

		if (m_notify) m_notify->OnDeviceRestored();
	}

	void GfxDevice::CreateFactory([[maybe_unused]] bool enableDebug)
	{
		KOI_LOG_TRACE("Creating D3D12 factory");
		u32 flags = 0;

#if KOI_DEBUG
		if (enableDebug)
		{
			ComPtr<ID3D12Debug> debugCtl;
			if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debugCtl))))
			{
				debugCtl->EnableDebugLayer();

				if (m_desc.EnableGPUValidation)
				{
					ComPtr<ID3D12Debug1> debug1;
					if (SUCCEEDED(debugCtl.As(&debug1)))
						debug1->SetEnableGPUBasedValidation(TRUE);
				}
			}
			else
			{
				KOI_LOG_WARN("D3D12 debug layer not available");
			}

			ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
			if (SUCCEEDED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
			{
				flags = DXGI_CREATE_FACTORY_DEBUG;
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);

				DXGI_INFO_QUEUE_MESSAGE_ID hide[] = { 80 /* swapchain output not controlled */ };
				DXGI_INFO_QUEUE_FILTER filter{};
				filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
				filter.DenyList.pIDList = hide;
				dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
			}
		}
#endif

		DX_CHECK(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_dxgiFactory)));

		// Tearing support
		BOOL allowTearing = FALSE;
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(m_dxgiFactory.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
		m_supportsTearing = (allowTearing == TRUE);
		if (!m_supportsTearing)
			KOI_LOG_WARN("Variable refresh rate / tearing not supported");
	}

	void GfxDevice::PickAdapter()
	{
		KOI_LOG_TRACE("Selecting D3D12 adapter");
		static constexpr D3D_FEATURE_LEVEL MIN_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

		ComPtr<IDXGIAdapter1> adapter;
		for (u32 i = 0;
			SUCCEEDED(m_dxgiFactory->EnumAdapterByGpuPreference(
				i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
			++i)
		{
			DXGI_ADAPTER_DESC1 desc{};
			DX_CHECK(adapter->GetDesc1(&desc));

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

			if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), MIN_FEATURE_LEVEL,
				__uuidof(ID3D12Device), nullptr)))
			{
				const std::string name = StringUtils::FromWide(desc.Description);
				KOI_LOG_INFO("D3D12 Adapter selected: VID:{:04X} PID:{:04X} - {}",
					desc.VendorId, desc.DeviceId, name);
				break;
			}
		}

		KOI_VERIFY_MSG(adapter, "No D3D12-capable adapter found");
		DX_CHECK(adapter.As(&m_adapter));
	}

	void GfxDevice::CreateDevice()
	{
		KOI_LOG_TRACE("Creating D3D12 device");
		static constexpr D3D_FEATURE_LEVEL MIN_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;
		DX_CHECK(::D3D12CreateDevice(m_adapter.Get(), MIN_FEATURE_LEVEL, IID_PPV_ARGS(&m_device)));
		m_device->SetName(L"Koi GfxDevice");

#if KOI_DEBUG
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(m_device.As(&infoQueue)))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

			D3D12_MESSAGE_ID hide[] =
			{
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			};
			D3D12_INFO_QUEUE_FILTER filter{};
			filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
			filter.DenyList.pIDList = hide;
			infoQueue->AddStorageFilterEntries(&filter);
		}
#endif

		g_featureSupport.Init(m_device.Get());
	}

	void GfxDevice::CreateQueues()
	{
		KOI_LOG_TRACE("Creating D3D12 queues");
		D3D12_COMMAND_QUEUE_DESC desc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
		};
		DX_CHECK(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_graphicsQueue)));
		m_graphicsQueue->SetName(L"Koi Graphics Queue");
	}

	void GfxDevice::CreateFences()
	{
		KOI_LOG_TRACE("Creating D3D12 fences");
		DX_CHECK(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFence)));
		m_frameFence->SetName(L"Koi Frame Fence");

		m_fenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		KOI_VERIFY_MSG(m_fenceEvent, "Failed to create fence event");
	}

	void GfxDevice::CreateHeaps()
	{
		KOI_LOG_TRACE("Creating D3D12 heaps");
		m_rtvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false);
		m_dsvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, false);
		m_resourceHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536, true);
		m_samplerHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 512, true);
	}

	void GfxDevice::FlushDeferredDeletions(u64 completedFence)
	{
		while (!m_deletionQueue.empty())
		{
			const PendingDeletion& p = m_deletionQueue.front();
			if (p.FenceValue > completedFence)
				break;

			switch (p.Type)
			{
			case PendingDeletion::ResourceType::Texture:
			{
				if (Texture* t = m_textures.Get(p.Texture))
				{
					m_rtvHeap.Free(t->RTVIndex);
					m_dsvHeap.Free(t->DSVIndex);
					m_resourceHeap.Free(t->SRVIndex);
					m_resourceHeap.Free(t->UAVIndex);
					// IsSwapChainImage: we must NOT release the ComPtr here — swapchain owns it.
					// The pool erase below will drop the ref, but since the swapchain also holds
					// one it stays alive.
				}
				KOI_LOG_TRACE("Texture resource deleted (Handle: idx={}, gen={})", p.Buffer.GetIndex(), p.Buffer.GetGeneration());
				m_textures.Erase(p.Texture);
				break;
			}

			case PendingDeletion::ResourceType::Buffer:
			{
				if (Buffer* b = m_buffers.Get(p.Buffer))
				{
					m_resourceHeap.Free(b->CBVIndex);
					m_resourceHeap.Free(b->SRVIndex);
					m_resourceHeap.Free(b->UAVIndex);
					if (b->MappedPtr && b->Resource)
						b->Resource->Unmap(0, nullptr);
				}
				KOI_LOG_TRACE("Buffer resource deleted (Handle: idx={}, gen={})", p.Buffer.GetIndex(), p.Buffer.GetGeneration());
				m_buffers.Erase(p.Buffer);
				break;
			}

			case PendingDeletion::ResourceType::Pipeline:
			{
				KOI_LOG_TRACE("Pipeline resource deleted (Handle: idx={}, gen={})", p.Pipeline.GetIndex(), p.Pipeline.GetGeneration());
				m_pipelines.Erase(p.Pipeline);
				break;
			}
			}
			m_deletionQueue.pop();
		}
	}
}
