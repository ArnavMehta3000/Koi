#include "Koi/Engine/Graphics/RenderPasses/AutoExposurePass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RenderScene.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <algorithm>
#include <cstring>

namespace koi
{
	namespace
	{
		constexpr ComputePipeline kClear  { .ComputeShader = "engine://Content/Shaders/HistogramClearCS" };
		constexpr ComputePipeline kBuild  { .ComputeShader = "engine://Content/Shaders/HistogramBuildCS" };
		constexpr ComputePipeline kReduce { .ComputeShader = "engine://Content/Shaders/ExposureReduceCS" };

		constexpr u64 kHistogramBytes = HISTOGRAM_BIN_COUNT * sizeof(u32);
		constexpr u64 kReadbackBytes  = kHistogramBytes + sizeof(f32);  // bins, then the exposure
	}

	AutoExposurePass::~AutoExposurePass()
	{
		if (!m_config.Device) return;

		for (const Readback& rb : m_readbacks)
		{
			if (rb.Buffer) m_config.Device->Destroy(rb.Buffer);
		}
	}

	void AutoExposurePass::CollectStats()
	{
		// This slot's last frame has retired by the time the slot comes round again
		Readback& rb = m_readbacks[m_config.Device->GetFrameSlot()];
		if (!rb.Pending) return;
		rb.Pending = false;

		const Buffer* buffer = m_config.Device->Get(rb.Buffer);
		if (!buffer || !buffer->MappedPtr) return;

		const byte* data = static_cast<const byte*>(buffer->MappedPtr);
		std::memcpy(m_stats.Bins.data(), data, kHistogramBytes);
		std::memcpy(&m_stats.Exposure, data + kHistogramBytes, sizeof(f32));
		m_stats.Settings = rb.Settings;
		m_stats.Valid    = true;
	}

	void AutoExposurePass::Record(RenderGraph& graph, const RenderView&, const RenderScene& renderScene)
	{
		ZoneScoped;
		RGBlackboard& bb = graph.GetBlackboard();

		const AutoExposureSettings settings = renderScene.GetPostProcess().AutoExposure;

		const SceneColor* scene = bb.TryGet<SceneColor>();
		if (!scene || !settings.Enabled)
		{
			m_stats.Valid = false;
			return;
		}

		const bool wantStats = m_statsRequested && m_config.Device;
		m_statsRequested = false;
		if (wantStats)
		{
			CollectStats();
		}

		const RGTexture   hdr  = scene->Color;
		const TextureDesc desc = graph.GetDesc(hdr);

		RGBuffer histogram = graph.CreateBuffer(BufferDesc{ .Size = kHistogramBytes, .Stride = sizeof(u32) }, "LuminanceHistogram");
		RGBuffer exposure  = graph.CreateBuffer(BufferDesc{ .Size = sizeof(f32), .Stride = sizeof(f32) }, "Exposure");

		const f32 minLog = settings.MinLogLuminance;
		const f32 range  = std::max(settings.MaxLogLuminance - minLog, 0.01f);

		graph.AddPass<ComputePass>("Clear Histogram",
			[&](RGBuilder& b) { histogram = b.Write(histogram); },
			[histogram](ComputeContext& ctx)
			{
				if (!ctx.SetPipeline(kClear)) return;
				ctx.SetRootConstants(HistogramRootConstants{ .HDRTextureIndex = INVALID_DESCRIPTOR, .HistogramIndex = ctx.UAV(histogram) });
				ctx.Dispatch(1);
			});

		graph.AddPass<ComputePass>("Build Histogram",
			[&](RGBuilder& b)
			{
				b.Read(hdr);
				histogram = b.Write(histogram);
			},
			[hdr, histogram, minLog, range, width = desc.Width, height = desc.Height](ComputeContext& ctx)
			{
				if (!ctx.SetPipeline(kBuild)) return;
				ctx.SetRootConstants(HistogramRootConstants
				{
					.HDRTextureIndex      = ctx.SRV(hdr),
					.HistogramIndex       = ctx.UAV(histogram),
					.MinLogLuminance      = minLog,
					.InvLogLuminanceRange = 1.0f / range,
				});

				const auto& group = ctx.GetThreadGroup();
				ctx.Dispatch((width + group[0] - 1) / group[0], (height + group[1] - 1) / group[1]);
			});

		graph.AddPass<ComputePass>("Reduce Exposure",
			[&](RGBuilder& b)
			{
				b.Read(histogram);
				exposure = b.Write(exposure);
			},
			[histogram, exposure, minLog, range, settings, pixels = static_cast<f32>(desc.Width * desc.Height)](ComputeContext& ctx)
			{
				if (!ctx.SetPipeline(kReduce)) return;
				ctx.SetRootConstants(ExposureRootConstants
				{
					.HistogramIndex       = ctx.SRV(histogram),
					.ExposureIndex        = ctx.UAV(exposure),
					.MinLogLuminance      = minLog,
					.LogLuminanceRange    = range,
					.PixelCount           = pixels,
					.KeyValue             = settings.KeyValue,
					.ExposureCompensation = settings.ExposureCompensation,
				});
				ctx.Dispatch(1);
			});

		if (wantStats)
		{
			Readback& rb = m_readbacks[m_config.Device->GetFrameSlot()];
			if (!rb.Buffer)
			{
				rb.Buffer = m_config.Device->CreateBuffer(BufferDesc{ .Size = kReadbackBytes, .Memory = MemoryUsage::GpuToCpu, .DebugName = "Exposure Readback" });
			}
			rb.Settings = settings;
			rb.Pending  = true;

			graph.AddPass<ComputePass>("Readback Exposure",
				[&](RGBuilder& b)
				{
					b.CopyFrom(histogram);
					b.CopyFrom(exposure);
				},
				[histogram, exposure, dst = rb.Buffer](ComputeContext& ctx)
				{
					ctx.CopyBuffer(dst, 0, histogram, kHistogramBytes);
					ctx.CopyBuffer(dst, kHistogramBytes, exposure, sizeof(f32));
				});
		}

		bb.Add(ExposureData{ .Exposure = exposure });
	}
}
