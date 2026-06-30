module;
#include <compare>
export module Koi.Graphics.RendererResources;

import Koi.Graphics.Types;

export namespace koi
{
	class GfxDevice;

	struct RendererResources
	{
		TextureHandle   WhiteTexture{};
		TextureHandle   FlatNormalTexture{};

		DescriptorIndex LinearWrapSampler  = INVALID_DESCRIPTOR;
		DescriptorIndex LinearClampSampler = INVALID_DESCRIPTOR;
		DescriptorIndex PointClampSampler  = INVALID_DESCRIPTOR;

		PipelineHandle  TonemapPipeline{};
		PipelineHandle  MeshOpaquePipeline{};
		PipelineHandle  MeshWireframePipeline{};

		void Init(GfxDevice& device);
		void Shutdown(GfxDevice& device);
		void RebuildPipelines(GfxDevice& device);
	};
}
