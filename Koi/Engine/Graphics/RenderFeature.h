#pragma once
#include "Koi/Engine/Graphics/RenderGraph/RenderGraph.h"
#include "Koi/Engine/Graphics/RenderPasses/PassData.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include <concepts>
#include <memory>
#include <string_view>
#include <vector>

namespace koi
{
	class World;

	// Something that has a name and a record function
	template <typename T>
	concept RenderFeature = requires(T& feature, RenderGraph& graph, const RenderView& view, World& world)
	{
		{ T::kName } -> std::convertible_to<std::string_view>;
		feature.Record(graph, view, world);
	};

	// The features of a pipeline, recorded in the order they were added
	class FeatureList : NonCopyable
	{
		struct Slot
		{
			virtual ~Slot() = default;
			virtual void Record(RenderGraph& graph, const RenderView& view, World& world) = 0;

			const void* Type = nullptr;
		};

		template <RenderFeature T>
		struct Model final : Slot
		{
			template <typename... Args>
			explicit Model(Args&&... args) : Feature(std::forward<Args>(args)...) { Type = &kTypeTag<T>; }

			void Record(RenderGraph& graph, const RenderView& view, World& world) override { Feature.Record(graph, view, world); }

			T Feature;
		};

		template <typename T>
		static constexpr char kTypeTag = 0;

	public:
		template <RenderFeature T, typename... Args>
		T& Add(Args&&... args)
		{
			auto& slot = m_features.emplace_back(std::make_unique<Model<T>>(std::forward<Args>(args)...));
			return static_cast<Model<T>&>(*slot).Feature;
		}

		template <RenderFeature T>
		[[nodiscard]] T* Find()
		{
			for (auto& slot : m_features)
			{
				if (slot->Type == &kTypeTag<T>)
					return &static_cast<Model<T>&>(*slot).Feature;
			}
			return nullptr;
		}

		void Record(RenderGraph& graph, const RenderView& view, World& world)
		{
			for (auto& slot : m_features)
			{
				slot->Record(graph, view, world);
			}
		}

		inline void Clear() { m_features.clear(); }

	private:
		std::vector<std::unique_ptr<Slot>> m_features;
	};
}
