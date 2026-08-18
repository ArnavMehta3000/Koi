#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Scripting/ScriptProperty.h"
#include <vector>

class asIScriptObject;
class asIScriptContext;
class asIScriptFunction;

namespace koi
{
	class GfxDevice;

	struct ScriptComponent
	{
		std::string ScriptPath;  // VFS path

		std::vector<ScriptProperty> Properties;

		asIScriptObject*   Instance   = nullptr;
		asIScriptContext*  Context    = nullptr;
		asIScriptFunction* OnUpdateFn = nullptr;
	};

	void LoadScript(Entity e, const toml::table& entityTable, std::string_view key, GfxDevice&);
	void SaveScript(Entity e, toml::table& entityTable, std::string_view key);
}
