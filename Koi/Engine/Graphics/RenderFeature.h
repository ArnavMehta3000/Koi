#pragma once
#include "Koi/Engine/Core/TypeIndex.h"
#include "Koi/Engine/RHI/RenderGraph/RenderGraph.h"
#include "Koi/Engine/Graphics/RenderPasses/PassData.h"
#include "Koi/Engine/Graphics/RenderView.h"
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace koi
{
	class RenderScene;

	// Something that has a name and a record function
	template <typename T>
	concept RenderFeature = requires(T& feature, RenderGraph& graph, const RenderView& view, const RenderScene& scene)
	{
		{ T::kName } -> std::convertible_to<std::string_view>;
		feature.Record(graph, view, scene);
	};

	// The features of a pipeline, recorded in the order they were added
	class FeatureList : NonCopyable
	{
		struct Slot
		{
			virtual ~Slot() = default;
			virtual void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene) = 0;

			u32              Type = ~0u;
			std::string_view Name;
		};

		template <RenderFeature T>
		struct Model final : Slot
		{
			template <typename... Args>
			explicit Model(Args&&... args) : Feature(std::forward<Args>(args)...)
			{
				Type = internal::TypeIndex<FeatureList, T>();
				Name = T::kName;
			}

			void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene) override { Feature.Record(graph, view, scene); }

			T Feature;
		};

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
				if (slot->Type == internal::TypeIndex<FeatureList, T>())
					return &static_cast<Model<T>&>(*slot).Feature;
			}
			return nullptr;
		}

		void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene)
		{
			for (auto& slot : m_features)
			{
				graph.SetGroup(slot->Name);
				slot->Record(graph, view, scene);
			}
			graph.SetGroup({});
		}

		[[nodiscard]] std::string Names() const
		{
			std::string names;
			for (const auto& slot : m_features)
			{
				if (!names.empty()) names += ", ";
				names += slot->Name;
			}
			return names;
		}

		inline void Clear() { m_features.clear(); }

	private:
		std::vector<std::unique_ptr<Slot>> m_features;
	};
}
