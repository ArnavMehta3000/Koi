#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Core/Error.h"
#include "Koi/Engine/ECS/ECS.h"
#include "Koi/Engine/Graphics/Camera.h"
#include <memory>
#include <optional>
#include <stacktrace>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace toml { inline namespace v3 { class table; } }

namespace koi
{
	class EntityBuilder;
	class IObserver;
	class ISystem;
	class RenderScene;
	struct PostProcess;

	class World
	{
	public:
		static constexpr f32 kFixedStep = 1.0f / 60.0f;

		World();
		~World();
		World(const World&) = delete;
		World& operator=(const World&) = delete;
		World(World&&) = delete;
		World& operator=(World&&) = delete;

		Result<void> Load(std::string_view virtualPath);
		void Load(const toml::table& scene, std::string_view sourcePath);
		[[nodiscard]] static World& From(const ECS& ecs) { return ecs.GetWorld(); }

		// Scene file order, then creation order
		[[nodiscard]] static std::pair<u64, u64> OrderKey(Entity e);

		// Creates an entity with a Transform. Names are unique per world and looked up by scene files, so runtime spawns should stay unnamed
		[[nodiscard]] EntityBuilder Spawn(std::string_view name = {});

		static void MakePersistent(Entity e);
		[[nodiscard]] static bool IsPersistent(Entity e);

		[[nodiscard]] inline const std::string& GetSourcePath()  const { return m_sourcePath; }
		[[nodiscard]] const BoundingBox&        GetBounds()      const;
		[[nodiscard]] inline ECS&               GetECS()               { return *m_ecs;       }
		[[nodiscard]] inline const ECS&         GetECS()         const { return *m_ecs;       }
		[[nodiscard]] inline const RenderScene& GetRenderScene() const { return *m_renderScene; }

		// Writes back to the scene file this world was loaded from
		Result<void> Save();
		[[nodiscard]] toml::table SaveToTable();
		void Clear();

		// Steps FixedUpdate systems as many times as dt covers, then runs the frame pipeline once
		void Update(f32 dt);

		void BeginPlay();
		void EndPlay();

		// Rebuilds every WorldTransform from the local Transforms
		void PropagateTransforms();

		// SetParent that keeps the child where it is in the world (null parent detaches)
		static void Reparent(Entity child, Entity parent);

		template<typename Event>
		void Emit(Entity e, const Event& event = {}) { m_ecs->Emit(e, event); }

		// The first failing system parks its exception here | Update rethrows it after the phase
		void ReportSystemFailure(std::string_view system, std::string_view what, std::stacktrace trace);
		[[nodiscard]] bool HasSystemFailure() const { return m_systemFailure.has_value(); }

		// Projects the primary camera entity for a surface of the given aspect and returns it
		// Null if the world has no primary camera
		[[nodiscard]] const Camera* ResolveCamera(f32 aspect);
		[[nodiscard]] const GPUMesh* GetMesh(MeshHandle handle) const;

		// The first PostProcess component in query order, else default
		[[nodiscard]] const PostProcess& ResolvePostProcess() const;


	private:
		void LoadFromTable(const toml::table& scene);
		void SwapScene(const toml::table& scene, std::string_view sourcePath);
		void StartSystems();
		void StopSystems();
		void ConfigureComponentHooks();
		void BuildSystems();
		void BuildObservers();

	private:
		struct SystemFailure
		{
			std::string     Message;
			std::stacktrace Trace;
		};

		std::vector<std::unique_ptr<ISystem>>   m_systems;
		std::vector<std::unique_ptr<IObserver>> m_observers;
		std::unique_ptr<RenderScene>            m_renderScene;
		std::unique_ptr<ECS>                    m_ecs;
		f32                                     m_fixedAccumulator = 0.0f;
		std::string                             m_sourcePath;        // virtual path this world was loaded from, empty if none
		Camera                                  m_camera;            // derived from the primary camera entity by ResolveCamera
		std::optional<SystemFailure>            m_systemFailure;
	};
}
