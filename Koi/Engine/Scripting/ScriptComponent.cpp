#include "Koi/Engine/Scripting/ScriptComponent.h"

namespace koi
{
	namespace
	{
		// toml::node -> native ScriptProperty, so nothing downstream of Load ever touches toml++
		std::optional<ScriptProperty> ToScriptProperty(const toml::key& key, const toml::node& node)
		{
			std::string name(key.str());

			if (const toml::value<bool>* asBool = node.as_boolean())
				return ScriptProperty{ .Name = std::move(name), .Type = ScriptPropertyType::Bool, .Value = asBool->get() };

			if (const toml::value<i64>* asInt = node.as_integer())
				return ScriptProperty{ .Name = std::move(name), .Type = ScriptPropertyType::Int, .Value = static_cast<i32>(asInt->get()) };

			if (const toml::value<f64>* asFloat = node.as_floating_point())
				return ScriptProperty{ .Name = std::move(name), .Type = ScriptPropertyType::Float, .Value = static_cast<f32>(asFloat->get()) };

			if (const toml::value<std::string>* asString = node.as_string())
				return ScriptProperty{ .Name = std::move(name), .Type = ScriptPropertyType::String, .Value = asString->get() };

			return std::nullopt;
		}

		void InsertScriptProperty(toml::table& table, const ScriptProperty& prop)
		{
			std::visit([&](const auto& value) { table.insert(prop.Name, value); }, prop.Value);
		}
	}

	void LoadScript(Entity e, const toml::table& entityTable, std::string_view key, GfxDevice&)
	{
		const toml::table* scriptTable = entityTable[key].as_table();
		if (!scriptTable) return;

		ScriptComponent component;
		component.ScriptPath = (*scriptTable)["path"].value_or(std::string());

		if (const toml::table* properties = (*scriptTable)["properties"].as_table())
		{
			for (auto&& [propKey, propNode] : *properties)
			{
				if (std::optional<ScriptProperty> prop = ToScriptProperty(propKey, propNode))
					component.Properties.push_back(std::move(*prop));
			}
		}

		e.set<ScriptComponent>(std::move(component));
	}

	void SaveScript(Entity e, toml::table& entityTable, std::string_view key)
	{
		const ScriptComponent* component = e.try_get<ScriptComponent>();
		if (!component) return;

		toml::table scriptTable;
		scriptTable.insert("path", component->ScriptPath);

		if (!component->Properties.empty())
		{
			toml::table properties;
			for (const ScriptProperty& prop : component->Properties)
			{
				InsertScriptProperty(properties, prop);
			}

			scriptTable.insert("properties", std::move(properties));
		}

		entityTable.insert(key, std::move(scriptTable));
	}
}
