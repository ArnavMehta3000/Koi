#pragma once
#include "Koi/Editor/Inspectors/ComponentInspector.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/ECS/Prefab.h"
#include "Koi/Engine/Core/ReflectSerializer.h"
#include <imgui.h>
#include <filesystem>
#include <imgui_stdlib.h>
#include <string>

// One ImGui widget per reflected field; components with custom widgets keep their own inspector
namespace koi::editor
{
	namespace utils
	{
		// "PopDuration" -> "Pop Duration"
		inline std::string FieldLabel(std::string_view name)
		{
			std::string out;
			for (size_t i = 0; i < name.size(); ++i)
			{
				if (i > 0 && std::isupper(static_cast<unsigned char>(name[i])))
					out += ' ';

				out += name[i];
			}
			return out;
		}

		template <typename T> inline constexpr bool kIsPrefabRef = false;
		template <typename P> inline constexpr bool kIsPrefabRef<PrefabRef<P>> = true;

		// Takes a dropped .prefab.toml
		template <typename P>
		bool DrawPrefabRef(const char* id, PrefabRef<P>& value)
		{
			const std::string_view path = value.GetPath();
			const std::string file      = std::filesystem::path(path).filename().string();
			const std::string name      = file.substr(0, file.size() - koi::utils::AssetExtensionOf(file).size());

			std::string dropped;
			if (!AssetField(id, AssetKindIcon(AssetKind::Prefab), name, path, dropped, true)) return false;

			if (dropped.empty())
			{
				value = {};
				return true;
			}

			if (const AssetTypeInfo* type = koi::utils::FindAssetTypeForFile(dropped); !type || type->Kind != AssetKind::Prefab)
			{
				KOI_LOG(Editor, Warn, "Dropped asset is not a prefab: {}", dropped);
				return false;
			}

			PrefabRef<P> loaded = PrefabRef<P>::Load(dropped);
			if (!loaded) return false;

			value = std::move(loaded);
			return true;
		}

		template <typename E> requires std::is_enum_v<E>
		bool DrawReflectedEnum(const char* label, E& value)
		{
			static constexpr auto entries = refl::EnumEntries<E>();
			int index = 0;
			for (size_t i = 0; i < entries.size(); ++i)
			{
				if (entries[i].Value == value) index = static_cast<int>(i);
			}

			const bool changed = ImGui::Combo(label, &index,
				[](void*, int i) { return entries[static_cast<size_t>(i)].Name.data(); },
				nullptr, static_cast<int>(entries.size()));

			if (changed) value = entries[static_cast<size_t>(index)].Value;
			return changed;
		}

		// One property-grid row per leaf; Transient<> and arrays recurse into rows of their own
		template <typename T>
		bool DrawReflectedField(const char* label, T& value)
		{
			if constexpr (refl::kIsTransient<T>)
			{
				ImGui::BeginDisabled();
				DrawReflectedField(label, value.get());
				ImGui::EndDisabled();
				return false;
			}
			else if constexpr (requires { std::tuple_size<T>::value; })  // std::array
			{
				bool changed = false;
				for (size_t i = 0; i < value.size(); ++i)
				{
					const std::string elem = std::string(label) + "[" + std::to_string(i) + "]";
					changed |= DrawReflectedField(elem.c_str(), value[i]);
				}
				return changed;
			}
			else
			{
				PropertyLabel(label);
				const std::string id = std::string("##") + label;

				if constexpr (std::is_same_v<T, bool>)             return ImGui::Checkbox(id.c_str(), &value);
				else if constexpr (std::is_same_v<T, f32>)         return ImGui::DragFloat(id.c_str(), &value, 0.01f);
				else if constexpr (std::is_same_v<T, i32>)         return ImGui::DragInt(id.c_str(), &value);
				else if constexpr (std::is_same_v<T, u64>)         return ImGui::InputScalar(id.c_str(), ImGuiDataType_U64, &value);
				else if constexpr (std::is_same_v<T, std::string>) return ImGui::InputText(id.c_str(), &value);
				else if constexpr (std::is_same_v<T, Vector3>)     return DragAxes(id.c_str(), &value.x, 3);
				else if constexpr (std::is_enum_v<T>)              return DrawReflectedEnum(id.c_str(), value);
				else if constexpr (kIsPrefabRef<T>)                return DrawPrefabRef(id.c_str(), value);
				else
				{
					ImGui::TextDisabled("(no widget)");
					return false;
				}
			}
		}

		// Every field as a row of its own property grid, so custom inspectors can call it anywhere
		template <typename T>
		bool DrawReflected(T& value)
		{
			if (!BeginPropertyGrid("##reflected")) return false;

			bool changed = false;
			refl::ForEachField(value, [&](const refl::FieldInfo& field, auto& member)
			{
				changed |= DrawReflectedField(FieldLabel(field.Name).c_str(), member);
			});
			EndPropertyGrid();
			return changed;
		}
	}

	// Drop-in inspector for a component that has no custom widgets
	template <typename T>
	class ReflectedInspector final : public ComponentInspector<T>
	{
	public:
		void Draw(Entity e) override
		{
			if (T* value = e.TryGet<T>()) utils::DrawReflected(*value);
		}
	};
}
