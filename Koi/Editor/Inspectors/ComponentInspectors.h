#pragma once
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/Math.h"
#include <string>

namespace koi::editor
{
	class InspectorRegistry;

	class TransformInspector final : public IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return "transform"; }
		void Draw(Entity e) override;

	private:
		// Euler angles are UI state cached until the selection changes
		Vector3    m_eulerDeg = Vector3::Zero;
		EntityID   m_eulerFor = 0;
		Quaternion m_eulerRot = Quaternion::Identity;
	};

	class MeshInspector final : public IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return "mesh"; }
		void Draw(Entity e) override;
	};

	class CameraInspector final : public IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return "camera"; }
		void Draw(Entity e) override;
	};

	class LightInspector final : public IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return "light"; }
		void Draw(Entity e) override;
	};

	void RegisterBuiltinInspectors(InspectorRegistry& registry);
}
