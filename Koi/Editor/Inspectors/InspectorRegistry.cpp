#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>

namespace koi::editor
{
	void InspectorRegistry::Add(std::unique_ptr<IComponentInspector> inspector)
	{
		if (!inspector) return;

		if (Find(inspector->Key()))
		{
			KOI_LOG(Editor, Error, "Component '{}' already has an inspector; ignoring the second one", inspector->Key());
			return;
		}

		KOI_LOG(Editor, Debug, "Registered inspector for component '{}'", inspector->Key());
		m_inspectors.push_back(std::move(inspector));
	}

	IComponentInspector* InspectorRegistry::Find(std::string_view key)
	{
		const auto it = std::ranges::find_if(m_inspectors,
			[key](const std::unique_ptr<IComponentInspector>& i) { return i->Key() == key; });

		return it != m_inspectors.end() ? it->get() : nullptr;
	}

	void InspectorRegistry::RegisterBuiltins()
	{
		Emplace<TransformInspector>();
		Emplace<MeshInspector>();
		Emplace<MaterialRefInspector>();
		Emplace<CameraInspector>();
		Emplace<LightInspector>();
	}
}
