#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Graphics/MeshRegistry.h"
#include <string>

namespace koi
{
	struct MeshRef
	{
		std::string SourcePath;  // VFS path of the GLTF this mesh was loaded from
		MeshHandle  Handle;      // lightweight handle into MeshRegistry
	};

	struct CameraComponent
	{
		f32  FovY    = 0.8f;     // vertical field of view, radians
		f32  Near    = 0.05f;
		f32  Far     = 1000.0f;
		bool Primary = true;     // the camera the world renders from
	};

	enum class LightType : u8
	{
		Directional,
		Point,
	};

	struct Light
	{
		LightType Type      = LightType::Point;
		Vector3   Color     = Vector3(1.0f, 1.0f, 1.0f);
		f32       Intensity = 1.0f;
		f32       Range     = 10.0f;  // point lights only (attenuation cutoff)
	};
}
