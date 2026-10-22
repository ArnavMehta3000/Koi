#include "Koi/Editor/Core/EditorExtension.h"
#include <vector>

namespace koi::editor
{
	namespace
	{
		std::vector<EditorExtensionRegistry::Entry>& Entries()
		{
			static std::vector<EditorExtensionRegistry::Entry> entries;
			return entries;
		}
	}

	bool EditorExtensionRegistry::Add(std::string_view name, Factory factory)
	{
		Entries().push_back(Entry{ .Name = name, .Create = factory });
		return true;
	}

	std::span<const EditorExtensionRegistry::Entry> EditorExtensionRegistry::All()
	{
		return Entries();
	}
}
