#pragma once
#include "Koi/Editor/Inspectors/ComponentInspector.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Math/Transform.h"

namespace koi::editor
{
	class InspectorRegistry;

	class TransformInspector final : public ComponentInspector<Transform>
	{
	public:
		void Draw(Entity e) override;

	private:
		// Euler angles are UI state cached until the selection changes
		Vector3    m_eulerDeg = Vector3::Zero;
		EntityID   m_eulerFor = 0;
		Quaternion m_eulerRot = Quaternion::Identity;
	};

	class MeshInspector final : public ComponentInspector<MeshRef>
	{
	public:
		void Draw(Entity e) override;
	};

	class MaterialRefInspector final : public ComponentInspector<MaterialRef>
	{
	public:
		void Draw(Entity e) override;
	};

	class CameraInspector final : public ComponentInspector<CameraComponent>
	{
	public:
		void Draw(Entity e) override;
	};

	class LightInspector final : public ComponentInspector<Light>
	{
	public:
		void Draw(Entity e) override;
	};

	class SkyInspector final : public ComponentInspector<Sky>
	{
	public:
		void Draw(Entity e) override;
	};

	class PostProcessInspector final : public ComponentInspector<PostProcess>
	{
	public:
		void Draw(Entity e) override;
	};
}
