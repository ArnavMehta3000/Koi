#pragma once
#include "Koi/Engine/ECS/World.h"
#include <string_view>

namespace koi
{
	class PreviewScene : NonCopyable, NonMovable
	{
	public:
		PreviewScene();
		~PreviewScene();

		void ShowMesh(std::string_view virtualPath);
		void ShowMaterial(std::string_view virtualPath);
		void Clear();
		
		void Update();  // Propagates transforms and syncs the RenderScene

		[[nodiscard]] World&             GetWorld()             { return m_world; }
		[[nodiscard]] const RenderScene& GetRenderScene() const { return m_world.GetRenderScene(); }
		[[nodiscard]] const BoundingBox& GetBounds()      const { return m_world.GetBounds(); }
		[[nodiscard]] bool               HasSubject()     const { return m_subject.IsAlive(); }

	private:
		World  m_world{ WorldKind::Preview };
		Entity m_subject;
	};
}
