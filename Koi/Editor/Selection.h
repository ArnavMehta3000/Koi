#pragma once
#include "Koi/Engine/ECS/World.h"
#include <string>

namespace koi::editor
{
	// Tag on the selected entity, so queries can filter on it. Never serialized
	struct Selected {};

	class Selection
	{
	public:
		inline void Select(Entity entity)
		{
			ClearEntity();
			m_asset.clear();
			if (entity.IsAlive())
			{
				entity.Add<Selected>();
				m_entity = entity;
			}
		}

		inline void SelectAsset(std::string virtualPath)
		{
			ClearEntity();
			m_asset = std::move(virtualPath);
		}

		inline void Clear()
		{
			ClearEntity();
			m_asset.clear();
		}

		[[nodiscard]] inline Entity GetEntity() const
		{
			return m_entity.IsAlive() && m_entity.Has<Selected>() ? m_entity : Entity{};
		}

		[[nodiscard]] inline const std::string& GetAsset() const { return m_asset; }
		[[nodiscard]] inline bool HasEntity() const { return GetEntity().IsAlive(); }
		[[nodiscard]] inline bool HasAsset()  const { return !m_asset.empty(); }

	private:
		inline void ClearEntity()
		{
			if (m_entity.IsAlive()) m_entity.Remove<Selected>();
			m_entity = Entity{};
		}

	private:
		Entity      m_entity;  // cached (may mismatch with tag, which is origin)
		std::string m_asset;   // virtual path
	};
}
