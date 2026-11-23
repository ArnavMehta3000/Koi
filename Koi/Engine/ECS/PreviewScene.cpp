#include "Koi/Engine/ECS/PreviewScene.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	static constexpr std::string_view kPreviewSphere = "engine://Content/Meshes/Sphere.kmesh";

	PreviewScene::PreviewScene()
	{
		// Same sun as a new scene
		m_world.Spawn("Key Light")
			.Rotated(math::SafeNormalize(Quaternion(-0.5f, 0.3f, 0.15f, 0.8f)))
			.With(Light
			{
				.Type         = LightType::Directional,
				.Color        = Vector3(1.0f, 0.95f, 0.9f),
				.Intensity    = 3.0f,
				.CastsShadows = true,
			});
	}

	PreviewScene::~PreviewScene()
	{
		m_world.Clear();  // Hands back the asset references the subject took
	}

	void PreviewScene::ShowMesh(std::string_view virtualPath)
	{
		Clear();
		m_subject = m_world.Spawn("Subject").WithMesh(virtualPath);
	}

	void PreviewScene::ShowMaterial(std::string_view virtualPath)
	{
		Clear();
		m_subject = m_world.Spawn("Subject").WithMesh(kPreviewSphere).WithMaterial(virtualPath);
	}

	void PreviewScene::Clear()
	{
		if (m_subject.IsAlive()) m_subject.Destroy();
		m_subject = {};
	}

	void PreviewScene::Update()
	{
		m_world.Update(0.0f);  // No sim (dt) for preview
	}
}
