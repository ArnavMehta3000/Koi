#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Core/Patterns/Match.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <comdef.h>
#include <cassert>
#include <ranges>
#if KOI_DEBUG
	#include <dxgidebug.h>
#endif

// Enable to log handle deletion logs
#define KOI_LOG_HANDLE_INFO 0
#if KOI_LOG_HANDLE_INFO
	#define LOG_HANDLE_TRACE(msg, h) KOI_LOG_TRACE(msg, (h).GetIndex(), (h).GetGeneration())
#else
	#define LOG_HANDLE_TRACE(msg, h) do {} while (false)
#endif



namespace koi
{
	namespace
	{
		CD3DX12FeatureSupport g_featureSupport;

		D3D12_RESOURCE_FLAGS ToD3DFlags(TextureUsageFlag flag)
		{
			D3D12_RESOURCE_FLAGS f = D3D12_RESOURCE_FLAG_NONE;
			if (flag.Has(TextureUsage::RenderTarget)) f |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			if (flag.Has(TextureUsage::DepthStencil)) f |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			if (flag.Has(TextureUsage::ShaderWrite))  f |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			if (flag.Has(TextureUsage::DepthStencil) && !flag.Has(TextureUsage::ShaderRead))
				f |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

			return f;
		}

		// A sampleable depth texture needs three different DXGI formats over one resource:
		// - A typeless resource (so it can carry both views)
		// - A typed depth DSV to render into
		// - A typed float SRV to sample
		struct DepthFormats { DXGI_FORMAT Resource; DXGI_FORMAT Dsv; DXGI_FORMAT Srv; };
		DepthFormats MapDepthFormats(Format f)
		{
			switch (f)
			{
			case Format::D32_FLOAT:         return { DXGI_FORMAT_R32_TYPELESS,   DXGI_FORMAT_D32_FLOAT,         DXGI_FORMAT_R32_FLOAT };
			case Format::D24_UNORM_S8_UINT: return { DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS };
			case Format::D16_UNORM:         return { DXGI_FORMAT_R16_TYPELESS,   DXGI_FORMAT_D16_UNORM,         DXGI_FORMAT_R16_UNORM };
			default:                        return { utils::ToDXGIFormat(f), utils::ToDXGIFormat(f), utils::ToDXGIFormat(f) };
			}
		}

		void ReportLiveObjects()
		{
#if KOI_DEBUG
			ZoneScoped;

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
	}

	// From DX12Common.h
	void internal::ThrowOnHRFail(HRESULT hr, const char* expr, const char* file, int line)
	{
		if (FAILED(hr))
		{
			_com_error err(hr);

			KOI_LOG_ERROR("DX_CHECK failed (0x{:08X} - {}): {} at {}:{}",
				static_cast<u32>(hr), utils::FromWide(err.ErrorMessage()), expr, file, line);

			assert(false && "D3D12/DXC/DXGI call failed");
		}
	}

	GfxDevice::GfxDevice(const GfxDevice::Desc& desc)
		: m_desc(desc)
	{
		ZoneScoped;

		CreateFactory(desc.EnableDebugLayer);
		PickAdapter();
		CreateDevice();
		CreateQueues();
		CreateFences();
		CreateHeaps();
		CreateRootSignature();

		m_textures.Reserve(4096);
		m_buffers.Reserve(8192);
		m_pipelines.Reserve(512);

		m_uploadContext.Init(this);
	}

	GfxDevice::~GfxDevice()
	{
		ZoneScoped;

		KOI_LOG_DEBUG("Destroying D3D12 device");

		m_uploadContext.Shutdown();
		WaitForIdle();

		// Drains outstanding timestamps and releases its query heap; must outlive nothing but
		// itself, and must go before the queue and device it was built against
		if (m_gpuProfilerCtx)
		{
			TracyD3D12Destroy(m_gpuProfilerCtx);
			m_gpuProfilerCtx = nullptr;
		}
		m_gpuTimers.Shutdown();

		FlushDeferredDeletions(~0ull);
		ReportLiveResources();

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
		KOI_LOG_DEBUG("Finished destroying D3D12 device");
	}

	// Anything still here outlived its owner's Destroy() call, or was never destroyed at all.
	void GfxDevice::ReportLiveResources()
	{
		std::vector<std::string_view> live;

		m_textures.ForEach([&](TextureHandle, Texture& t)
		{
			if (!t.IsSwapChainImage)
				live.push_back(t.Desc.DebugName.empty() ? "<unnamed texture>" : t.Desc.DebugName);
		});

		m_buffers.ForEach([&](BufferHandle, Buffer& b)
		{
			live.push_back(b.Desc.DebugName.empty() ? "<unnamed buffer>" : b.Desc.DebugName);
		});

		m_pipelines.ForEach([&](PipelineHandle, Pipeline&) { live.push_back("<pipeline>"); });

		if (live.empty())
			return;

		KOI_LOG_WARN("{} GPU resource(s) still live at device destruction:", live.size());
		for (const std::string_view name : live)
		{
			KOI_LOG_WARN("\t{}", name);
		}
	}

	u64 GfxDevice::GetCompletedFenceValue() const noexcept
	{
		return m_frameFence ? m_frameFence->GetCompletedValue() : 0;
	}

	TextureHandle GfxDevice::RegisterSwapChainImage(ComPtr<dx12::Resource> resource, const TextureDesc& desc)
	{
		ZoneScoped;

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
		ZoneScoped;

		Texture tex{ .Desc = desc, .IsSwapChainImage = false };

		const bool isDepth        = desc.Usage.Has(TextureUsage::DepthStencil);
		const bool isRenderTarget = desc.Usage.Has(TextureUsage::RenderTarget);
		const bool isShaderRead   = desc.Usage.Has(TextureUsage::ShaderRead);

		const DepthFormats depthFmts     = MapDepthFormats(desc.Format);
		const DXGI_FORMAT resourceFormat = (isDepth && isShaderRead)
			? depthFmts.Resource
			: utils::ToDXGIFormat(desc.Format);

		auto rd = CD3DX12_RESOURCE_DESC::Tex2D(
			resourceFormat,
			desc.Width, desc.Height,
			static_cast<u16>(desc.DepthOrArray),
			static_cast<u16>(desc.MipLevels),
			desc.SampleCount, 0U,  // [NOTE]: Sample quality is 0
			ToD3DFlags(desc.Usage),
			D3D12_TEXTURE_LAYOUT_UNKNOWN
		);

		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

		// Debug layer will warn if I pass a different clear value
		D3D12_CLEAR_VALUE clearValue{};
		const D3D12_CLEAR_VALUE* pClearValue = nullptr;

		if (isDepth)
		{
			clearValue.Format               = utils::ToDXGIFormat(desc.Format);
			clearValue.DepthStencil.Depth   = 1.0f;
			clearValue.DepthStencil.Stencil = 0;
			pClearValue                     = &clearValue;
		}
		else if (isRenderTarget)
		{
			clearValue.Format   = utils::ToDXGIFormat(desc.Format);
			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 1.0f;
			pClearValue         = &clearValue;
		}

		DX_CHECK(m_device->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &rd,
			initialState, pClearValue, IID_PPV_ARGS(&tex.Resource)));

		tex.CurrentState = initialState;
		if (!desc.DebugName.empty())
		{
			const std::wstring wname = utils::ToWide(desc.DebugName);
			tex.Resource->SetName(wname.c_str());
		}

		if (isDepth)
		{
			tex.DSVIndex = m_dsvHeap.Allocate();

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc
			{
				.Format        = utils::ToDXGIFormat(desc.Format),
				.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
				.Flags         = D3D12_DSV_FLAG_NONE,
			};
			m_device->CreateDepthStencilView(tex.Resource.Get(), &dsvDesc, m_dsvHeap.CpuHandle(tex.DSVIndex));
		}

		// Bindless: sampled textures get an SRV addressable via ResourceDescriptorHeap[].
		if (desc.Usage.Has(TextureUsage::ShaderRead))
		{
			tex.SRVIndex = m_resourceHeap.Allocate();

			// Depth resources are typeless, so the SRV must use the typed float view format
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc
			{
				.Format                  = isDepth ? depthFmts.Srv : utils::ToDXGIFormat(desc.Format),
				.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D               = { .MostDetailedMip = 0, .MipLevels = desc.MipLevels },
			};
			m_device->CreateShaderResourceView(tex.Resource.Get(), &srvDesc, m_resourceHeap.CpuHandle(tex.SRVIndex));
		}

		if (isRenderTarget)
		{
			tex.RTVIndex = m_rtvHeap.Allocate();

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
			{
				.Format        = utils::ToDXGIFormat(desc.Format),
				.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			};
			m_device->CreateRenderTargetView(tex.Resource.Get(), &rtvDesc, m_rtvHeap.CpuHandle(tex.RTVIndex));
		}

		KOI_LOG_TRACE("Created texture '{}' ({}x{}, {} mip(s))",
			desc.DebugName.empty() ? "<unnamed>" : desc.DebugName, desc.Width, desc.Height, desc.MipLevels);

		return m_textures.Emplace(std::move(tex));
	}

	BufferHandle GfxDevice::CreateBuffer(const BufferDesc& desc)
	{
		ZoneScoped;

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

		// Align 256 if its a constant buffer
		const u64 allocSize = desc.Usage.Has(BufferUsage::Constant)
			? math::AlignGPU256(desc.Size)
			: desc.Size;

		CD3DX12_HEAP_PROPERTIES heap(heapType);
		CD3DX12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(allocSize);

		// UPLOAD/READBACK heaps cannot host UAV (or RTV) resources - only add the UAV flag for
		// GPU-local Storage buffers; CPU-mapped Storage buffers (e.g. per-frame upload buffers) still
		// get their SRV below without it, since a UAV isn't needed for read-only structured-buffer access
		if (desc.Usage.Has(BufferUsage::Storage) && desc.Memory == MemoryUsage::GpuOnly)
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

		// Bindless: structured-buffer SRV so the buffer is addressable via ResourceDescriptorHeap[]
		if (desc.Usage.Has(BufferUsage::Storage) && desc.Stride > 0)
		{
			buf.SRVIndex = m_resourceHeap.Allocate();

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc
			{
				.Format                  = DXGI_FORMAT_UNKNOWN,
				.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer =
				{
					.FirstElement        = 0,
					.NumElements         = static_cast<u32>(desc.Size / desc.Stride),
					.StructureByteStride = desc.Stride,
					.Flags               = D3D12_BUFFER_SRV_FLAG_NONE,
				},
			};
			m_device->CreateShaderResourceView(buf.Resource.Get(), &srvDesc, m_resourceHeap.CpuHandle(buf.SRVIndex));
		}

		// Bindless: CBV so the buffer is addressable via ResourceDescriptorHeap[] as ConstantBuffer<T>.
		if (desc.Usage.Has(BufferUsage::Constant))
		{
			buf.CBVIndex = m_resourceHeap.Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc
			{
				.BufferLocation = buf.GPUAddress,
				.SizeInBytes    = static_cast<u32>(allocSize),
			};
			m_device->CreateConstantBufferView(&cbvDesc, m_resourceHeap.CpuHandle(buf.CBVIndex));
		}

		if (!desc.DebugName.empty())
		{
			const std::wstring wname = utils::ToWide(desc.DebugName);
			buf.Resource->SetName(wname.c_str());
		}

		KOI_LOG_TRACE("Created buffer '{}' ({} bytes, stride {})",
			desc.DebugName.empty() ? "<unnamed>" : desc.DebugName, allocSize, desc.Stride);

		return m_buffers.Emplace(std::move(buf));
	}

	void GfxDevice::Destroy(TextureHandle h)
	{
		ZoneScoped;

		if (m_textures.IsValid(h))
		{
			m_deletionQueue.push({ m_nextFenceValue, h });
			LOG_HANDLE_TRACE("Marked texture for deletion (Handle: idx={}, gen={})", h);
		}
		else
		{
			KOI_LOG_WARN("Trying to mark invalid texture for deletion (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		}
	}

	void GfxDevice::Destroy(BufferHandle h)
	{
		ZoneScoped;

		if (m_buffers.IsValid(h))
		{
			m_deletionQueue.push({ m_nextFenceValue, h });
			LOG_HANDLE_TRACE("Marked buffer for deletion (Handle: idx={}, gen={})", h);
		}
		else
		{
			KOI_LOG_WARN("Trying to mark invalid buffer for deletion (Handle: {})", h.GetId());
		}
	}

	void GfxDevice::GpuProfilerNewFrame()
	{
		m_gpuTimers.BeginFrame(GetCompletedFenceValue());

		DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
		if (SUCCEEDED(m_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory)))
		{
			FrameStats::Count("VRAM MB",        static_cast<i64>(memory.CurrentUsage >> 20));
			FrameStats::Count("VRAM Budget MB", static_cast<i64>(memory.Budget >> 20));
		}
		if (m_gpuProfilerCtx)
		{
			TracyD3D12NewFrame(m_gpuProfilerCtx);
		}
	}

	void GfxDevice::GpuProfilerCollect()
	{
		m_gpuTimers.EndFrame(m_nextFenceValue - 1);  // the value Present just signalled
		if (m_gpuProfilerCtx)
		{
			ZoneScoped;
			TracyD3D12Collect(m_gpuProfilerCtx);
		}
	}

	u64 GfxDevice::SignalFrame()
	{
		ZoneScoped;

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
		ZoneScoped;

		/// DevNote: This function checks if the GPU has already passed that (signalled) value
		/// If not, then it uses `SetEventOnCompletion` + `WaitForSingleObject` to block the CPU until it has
		/// This is the DX12 equivalent of DX11' `Flush()` stall, except I have the control here

		if (m_frameFence->GetCompletedValue() >= fenceValue) return;
		DX_CHECK(m_frameFence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		::WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	void GfxDevice::WaitForIdle()
	{
		ZoneScoped;

		///DevNote: This is the nuclear option, it signal a new value and immediately waits for it
		/// draining the entire GPU pipeline. Used before resize and shutdown

		const u64 value = m_nextFenceValue++;
		DX_CHECK(m_graphicsQueue->Signal(m_frameFence.Get(), value));
		KOI_LOG_TRACE("WaitForIdle: draining GPU to fence {}", value);
		WaitForFence(value);
	}

	void GfxDevice::FlushPendingDeletions()
	{
		FlushDeferredDeletions(~0ull);
	}

	void GfxDevice::Destroy(PipelineHandle h)
	{
		ZoneScoped;

		if (m_pipelines.IsValid(h))
		{
			m_deletionQueue.push({ m_nextFenceValue, h });
			LOG_HANDLE_TRACE("Marked pipeline for deletion (Handle: idx={}, gen={})", h);
		}
		else
		{
			KOI_LOG_WARN("Trying to mark invalid pipeline for deletion (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		}
	}

	void GfxDevice::FreeResource(TextureHandle h)
	{
		ZoneScoped;

		if (Texture* t = m_textures.Get(h))
		{
			m_rtvHeap.Free(t->RTVIndex);
			m_dsvHeap.Free(t->DSVIndex);
			m_resourceHeap.Free(t->SRVIndex);
			m_resourceHeap.Free(t->UAVIndex);
		}
	}

	void GfxDevice::FreeResource(BufferHandle h)
	{
		ZoneScoped;

		if (Buffer* b = m_buffers.Get(h))
		{
			m_resourceHeap.Free(b->CBVIndex);
			m_resourceHeap.Free(b->SRVIndex);
			m_resourceHeap.Free(b->UAVIndex);
			if (b->MappedPtr && b->Resource)
				b->Resource->Unmap(0, nullptr);
		}
	}

	ComPtr<dx12::PipelineState> GfxDevice::BuildPipelineState(const PipelineDesc& desc)
	{
		ZoneScoped;

		assert(!desc.VertexShader.empty() && "PipelineDesc missing vertex shader");
		assert((!desc.PixelShader.empty() || desc.RenderTargetCount == 0) && "PipelineDesc missing pixel shader");  // Depth-only pipelines (shadow maps, depth prepass) have no pixel shader at all
		assert(desc.RenderTargetCount <= MAX_RENDER_TARGETS && "Too many render targets");
		assert(desc.RenderTargetCount > 0 || desc.DepthFormat != Format::UNKNOWN && "PipelineDesc has neither render targets nor a depth target");

		// Vertex input layout
		std::array<D3D12_INPUT_ELEMENT_DESC, MAX_VERTEX_ATTRIBUTES> inputElements{};

		for (const auto& [index, attr] : std::views::enumerate(desc.VertexLayout) | std::views::as_const)
		{
			assert(index < MAX_VERTEX_ATTRIBUTES && "Too many vertex attributes");
			assert(!attr.SemanticName.empty() && "Vertex attribute missing semantic name");

			inputElements[index] =
			{
				.SemanticName         = attr.SemanticName.data(),
				.SemanticIndex        = attr.SemanticIndex,
				.Format               = utils::ToDXGIFormat(attr.Format),
				.InputSlot            = attr.InputSlot,
				.AlignedByteOffset    = attr.ByteOffset,
				.InputSlotClass       = attr.PerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = attr.PerInstance ? 1u : 0u,
			};
		}

		// Rasterizer
		auto rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
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
		auto blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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
		auto depthDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		depthDesc.DepthEnable              = desc.DepthStencil.DepthEnable;
		depthDesc.DepthWriteMask           = desc.DepthStencil.DepthWrite
			? D3D12_DEPTH_WRITE_MASK_ALL
			: D3D12_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc                = desc.DepthStencil.DepthFunc;
		depthDesc.StencilEnable            = desc.DepthStencil.StencilEnable;

		// Create PSO desc
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc
		{
			.pRootSignature        = m_rootSignature.Get(),
			.VS                    = { desc.VertexShader.data(), desc.VertexShader.size() },
			.PS                    = { desc.PixelShader.data(), desc.PixelShader.size() },
			.BlendState            = blendDesc,
			.SampleMask            = 0xffffffff , // Same as UINT_MAX
			.RasterizerState       = rasterDesc,
			.DepthStencilState     = depthDesc,
			.InputLayout           = { inputElements.data(), static_cast<u32>(desc.VertexLayout.size())},
			.PrimitiveTopologyType = desc.TopologyType,
			.NumRenderTargets      = desc.RenderTargetCount,
			.DSVFormat             = utils::ToDXGIFormat(desc.DepthFormat),
			.SampleDesc            = { desc.SampleCount , 0 },
		};

		// Add optional shaders
		if (!desc.GeometryShader.empty()) psoDesc.GS = { desc.GeometryShader.data(), desc.GeometryShader.size() };
		if (!desc.HullShader.empty())     psoDesc.HS = { desc.HullShader.data(), desc.HullShader.size()         };
		if (!desc.DomainShader.empty())   psoDesc.DS = { desc.DomainShader.data(), desc.DomainShader.size()     };

		for (u32 i = 0; i < desc.RenderTargetCount; i++)
		{
			psoDesc.RTVFormats[i] = utils::ToDXGIFormat(desc.RenderTargetFormats[i]);
		}

		// Create pipeline/PSO
		ComPtr<dx12::PipelineState> pso;
		DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		if (pso && !desc.DebugName.empty())
		{
			const std::wstring wname = utils::ToWide(std::string(desc.DebugName));
			pso->SetName(wname.c_str());
		}

		return pso;
	}

	PipelineHandle GfxDevice::CreatePipeline(const PipelineDesc& desc)
	{
		ComPtr<dx12::PipelineState> pso = BuildPipelineState(desc);
		if (!pso)
			return {};

		KOI_LOG_DEBUG("Created pipeline: {}", desc.DebugName.empty() ? "<unnamed>" : desc.DebugName);
		return m_pipelines.Emplace(Pipeline{ .Pso = std::move(pso), .Topology = desc.Topology });
	}

	bool GfxDevice::RecreatePipeline(PipelineHandle h, const PipelineDesc& desc)
	{
		if (!m_pipelines.Get(h))  // Handle is invalid
			return false;

		// Rebuild pipeline
		ComPtr<dx12::PipelineState> pso = BuildPipelineState(desc);
		if (!pso)
			return false;

		// Overwrite old pipeline
		Pipeline* pipeline = m_pipelines.Get(h);  assert(pipeline);
		pipeline->Pso      = std::move(pso);
		pipeline->Topology = desc.Topology;

		KOI_LOG_DEBUG("Recreated pipeline: {}", desc.DebugName.empty() ? "<unnamed>" : desc.DebugName);
		return true;
	}

	DescriptorIndex GfxDevice::CreateSampler(const D3D12_SAMPLER_DESC& desc)
	{
		ZoneScoped;

		const DescriptorIndex index = m_samplerHeap.Allocate();
		m_device->CreateSampler(&desc, m_samplerHeap.CpuHandle(index));
		return index;
	}

	void GfxDevice::CreateRootSignature()
	{
		ZoneScoped;

		KOI_LOG_DEBUG("Creating bindless root signature");

		const D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion = g_featureSupport.HighestRootSignatureVersion();
		assert(rootSignatureVersion >= D3D_ROOT_SIGNATURE_VERSION_1_1 && "Bindless requires root signature version 1.1 or higher");

		auto rootParam = CD3DX12_ROOT_PARAMETER1();
		rootParam.InitAsConstants(MAX_PUSH_CONSTANT_U32S, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

		auto desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC();
		desc.Init_1_1(
			1, &rootParam,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  |  // For bindless
			D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED			// For bindless
		);

		ComPtr<ID3DBlob> blob, error;
		HRESULT hr = ::D3DX12SerializeVersionedRootSignature(&desc, rootSignatureVersion, &blob, &error);
		if (FAILED(hr) && error)
		{
			KOI_LOG_ERROR("Failed to serialize bindless root signature: {}",
				static_cast<const char*>(error->GetBufferPointer()));
		}
		DX_CHECK(hr);

		DX_CHECK(m_device->CreateRootSignature(
			0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature)));

		m_rootSignature->SetName(L"Bindless Root Signature");
	}

	void GfxDevice::HandleDeviceLost()
	{
		ZoneScoped;

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

		KOI_LOG_INFO("D3D12 device recreated after device loss");
		if (m_notify) m_notify->OnDeviceRestored();
	}

	void GfxDevice::CreateFactory([[maybe_unused]] bool enableDebug)
	{
		ZoneScoped;

		KOI_LOG_DEBUG("Creating D3D12 factory");
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
		{
			KOI_LOG_WARN("Variable refresh rate / tearing not supported");
		}
	}

	void GfxDevice::PickAdapter()
	{
		ZoneScoped;

		KOI_LOG_DEBUG("Selecting D3D12 adapter");
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
				const std::string name = utils::FromWide(desc.Description);
				KOI_LOG_INFO("D3D12 Adapter selected: VID:{:04X} PID:{:04X} - {}",
					desc.VendorId, desc.DeviceId, name);
				break;
			}
		}

		assert(adapter && "No D3D12-capable adapter found");
		DX_CHECK(adapter.As(&m_adapter));
	}

	void GfxDevice::CreateDevice()
	{
		ZoneScoped;

		ComPtr<ID3D12DeviceRemovedExtendedDataSettings2> dredSettings;
		DX_CHECK(::D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings)));

		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

		KOI_LOG_DEBUG("Creating D3D12 device");
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
		ZoneScoped;

		KOI_LOG_DEBUG("Creating D3D12 queues");
		D3D12_COMMAND_QUEUE_DESC desc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
		};
		DX_CHECK(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_graphicsQueue)));
		m_graphicsQueue->SetName(L"Koi Graphics Queue");

		// Owns a timestamp query heap + readback buffer; only DIRECT/COMPUTE queues can carry
		// timestamps, so the copy queue in UploadContext is deliberately left uninstrumented
		m_gpuProfilerCtx = TracyD3D12Context(m_device.Get(), m_graphicsQueue.Get());
		TracyD3D12ContextName(m_gpuProfilerCtx, "Graphics", 8);

		m_gpuTimers.Init(m_device.Get(), m_graphicsQueue.Get());
	}

	void GfxDevice::CreateFences()
	{
		ZoneScoped;

		KOI_LOG_DEBUG("Creating D3D12 fences");
		DX_CHECK(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFence)));
		m_frameFence->SetName(L"Koi Frame Fence");

		m_fenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		assert(m_fenceEvent && "Failed to create fence event");
	}

	void GfxDevice::CreateHeaps()
	{
		ZoneScoped;

		KOI_LOG_DEBUG("Creating D3D12 heaps");

		constexpr u32 rtvHeapSize      = 256;
		constexpr u32 dsvHeapSize      = 64;
		constexpr u32 resourceHeapSize = 16384;
		constexpr u32 samplerHeapSize  = 128;

		auto dev = m_device.Get();

		m_rtvHeap.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtvHeapSize, false);
		m_dsvHeap.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsvHeapSize, false);
		m_resourceHeap.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, resourceHeapSize, true);
		m_samplerHeap.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, samplerHeapSize, true);

		KOI_LOG_DEBUG("Descriptor heaps: RTV={} DSV={} CBV/SRV/UAV={} Sampler={}",
			rtvHeapSize, dsvHeapSize, resourceHeapSize, samplerHeapSize);
	}

	void GfxDevice::FlushDeferredDeletions(u64 completedFence)
	{
		ZoneScoped;

		while (!m_deletionQueue.empty())
		{
			const PendingDeletion& p = m_deletionQueue.front();
			if (p.FenceValue > completedFence)
				break;

			std::visit(Match
			{
				[this](TextureHandle h)
				{
					// IsSwapChainImage: we must NOT release the ComPtr here - swapchain owns it.
					// The pool erase below will drop the ref, but since the swapchain also holds
					// one it stays alive.
					FreeResource(h);
					LOG_HANDLE_TRACE("Texture resource deleted (Handle: idx={}, gen={})", h);
					m_textures.Erase(h);
				},
				[this](BufferHandle h)
				{
					FreeResource(h);
					LOG_HANDLE_TRACE("Buffer resource deleted (Handle: idx={}, gen={})", h);
					m_buffers.Erase(h);
				},
				[this](PipelineHandle h)
				{
					LOG_HANDLE_TRACE("Pipeline resource deleted (Handle: idx={}, gen={})", h);
					m_pipelines.Erase(h);
				},
			}, p.Handle);
			m_deletionQueue.pop();
		}
	}
}
