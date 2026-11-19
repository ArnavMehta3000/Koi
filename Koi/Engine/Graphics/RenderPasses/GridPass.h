#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Grid.hlsli"
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/RenderFeature.h"

namespace koi
{
	class  GfxDevice;
	struct RendererResources;

	class GridPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
		};

		struct Settings
		{
			static constexpr auto kKey     = "Grid";

			bool    Enabled        = false;
			f32     CellSize       = 1.0f;
			f32     ThickEvery     = 10.0f;
			f32     FadeStart      = 40.0f;
			f32     FadeEnd        = 120.0f;
			f32     MajorLineWidth = 1.8f;   // pixels; minor lines are always 1

			Vector4 ThinColor  { 0.42f, 0.44f, 0.50f, 0.20f };
			Vector4 ThickColor { 0.82f, 0.85f, 0.92f, 0.72f };

			Vector4 AxisXColor { 0.91f, 0.26f, 0.30f, 1.00f };  // Red
			Vector4 AxisZColor { 0.24f, 0.51f, 0.96f, 1.00f };  // Blue

			KOI_REFLECT(Settings, Enabled, CellSize, ThickEvery, FadeStart, FadeEnd, MajorLineWidth, ThinColor, ThickColor, AxisXColor, AxisZColor);
		};

		explicit GridPass(const Config& config);

		static constexpr std::string_view kName = "Grid";

		// Draws into DisplayColor and SceneDepth; adds nothing while disabled
		void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene);

		[[nodiscard]] inline Settings& GetSettings() noexcept { return m_settings; }
		[[nodiscard]] inline const Settings& GetSettings() const noexcept { return m_settings; }

	private:
		Config                       m_config;
		Settings                     m_settings{};
		GpuFrameBuffer<GridConstants> m_constants;
	};
}
