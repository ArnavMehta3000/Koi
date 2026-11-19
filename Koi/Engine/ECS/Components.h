#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Math/MathTypes.h"
#include <string_view>

namespace koi
{
	struct MeshRef
	{
		static constexpr std::string_view kKey = "Mesh";
		static constexpr std::string_view kCategory = "Rendering";

		MeshHandle Handle;  // the source path lives in the mesh AssetCache, not per entity
	};

	// Position in the scene file, which is also the outliner order
	struct SceneOrder
	{
		u32 Index = 0;
	};

	// Runtime-only tag (World::MakePersistent)
	struct Persistent {};

	// Overrides every material slot of the entity's mesh with one material asset
	struct MaterialRef
	{
		static constexpr std::string_view kKey = "Material";
		static constexpr std::string_view kCategory = "Rendering";

		MaterialHandle Handle;
	};

	struct CameraComponent
	{
		static constexpr std::string_view kKey = "Camera";
		static constexpr std::string_view kCategory = "Rendering";

		f32  FovY    = 0.8f;     // vertical field of view, radians
		f32  Near    = 0.05f;
		f32  Far     = 1000.0f;
		bool Primary = true;     // the camera the world renders from

		KOI_REFLECT(CameraComponent, FovY, Near, Far, Primary);
	};

	struct AutoExposureSettings
	{
		bool Enabled              = true;
		f32  MinLogLuminance      = -8.0f;
		f32  MaxLogLuminance      = 4.0f;
		f32  KeyValue             = 0.18f;  // the average luminance the frame is exposed to
		f32  ExposureCompensation = 0.0f;   // EV

		bool operator==(const AutoExposureSettings&) const = default;

		KOI_REFLECT(AutoExposureSettings, Enabled, MinLogLuminance, MaxLogLuminance, KeyValue, ExposureCompensation);
	};

	// Values match the TONEMAPPER_* constants in Shaders/Interop/PostProcess.hlsli
	enum class Tonemapper : u8
	{
		ACES,
		AgX,
		PBRNeutral,
		Hable,
		Reinhard,
		None,  // clamped, for checking exposure on its own
	};

	struct TonemapSettings
	{
		Tonemapper Operator = Tonemapper::ACES;

		bool operator==(const TonemapSettings&) const = default;

		KOI_REFLECT(TonemapSettings, Operator);
	};

	struct PostProcess
	{
		static constexpr std::string_view kKey = "PostProcess";
		static constexpr std::string_view kCategory = "Rendering";

		AutoExposureSettings AutoExposure;
		TonemapSettings      Tonemap;

		KOI_REFLECT(PostProcess, AutoExposure, Tonemap);
	};

	enum class LightType : u8
	{
		Directional,
		Point,
	};

	struct Light
	{
		static constexpr std::string_view kKey = "Light";
		static constexpr std::string_view kCategory = "Rendering";

		LightType Type      = LightType::Point;
		Vector3   Color     = Vector3(1.0f, 1.0f, 1.0f);
		f32       Intensity = 1.0f;
		f32       Range     = 10.0f;  // point lights only (attenuation cutoff)

		// Directional only. The first caster in query order is the one ShadowPass renders
		bool      CastsShadows = false;
		f32       ShadowBias   = 0.0015f;  // constant offset in light-space depth
		f32       ShadowExtent = 0.0f;     // half-size of the ortho box, centered on the entity; 0 fits the scene bounds instead

		KOI_REFLECT(Light, Type, Color, Intensity, Range, CastsShadows, ShadowBias, ShadowExtent);
	};
}
