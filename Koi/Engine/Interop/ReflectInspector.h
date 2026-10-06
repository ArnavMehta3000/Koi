#pragma once
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <rfl/enums.hpp>
#include <rfl/to_view.hpp>
#include <string>

// One ImGui widget per reflected field; components with custom widgets keep their own inspector
namespace koi
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

		template <typename E> requires std::is_enum_v<E>
		bool DrawReflectedEnum(const char* label, E& value)
		{
			static constexpr auto entries = rfl::get_enumerator_array<E>();
			int index = 0;
			for (size_t i = 0; i < entries.size(); ++i)
			{
				if (entries[i].second == value) index = static_cast<int>(i);
			}

			const bool changed = ImGui::Combo(label, &index,
				[](void*, int i) { return entries[static_cast<size_t>(i)].first.data(); },
				nullptr, static_cast<int>(entries.size()));

			if (changed) value = entries[static_cast<size_t>(index)].second;
			return changed;
		}

		template <typename T>
		bool DrawReflectedField(const char* label, T& value)
		{
			if constexpr (std::is_same_v<T, bool>)             return ImGui::Checkbox(label, &value);
			else if constexpr (std::is_same_v<T, f32>)         return ImGui::DragFloat(label, &value, 0.01f);
			else if constexpr (std::is_same_v<T, i32>)         return ImGui::DragInt(label, &value);
			else if constexpr (std::is_same_v<T, u64>)         return ImGui::InputScalar(label, ImGuiDataType_U64, &value);
			else if constexpr (std::is_same_v<T, std::string>) return ImGui::InputText(label, &value);
			else if constexpr (std::is_same_v<T, Vector3>)     return ImGui::DragFloat3(label, &value.x, 0.05f);
			else if constexpr (std::is_enum_v<T>)              return DrawReflectedEnum(label, value);
			else if constexpr (requires { typename T::Type; value.get(); })  // rfl::Skip<> runtime state
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
				ImGui::TextDisabled("%s: (no widget)", label);
				return false;
			}
		}

		template <typename T>
		bool DrawReflected(T& value)
		{
			bool changed = false;
			rfl::to_view(value).apply([&](const auto& field)
			{
				changed |= DrawReflectedField(FieldLabel(field.name()).c_str(), *field.value());
			});
			return changed;
		}
	}

	// Drop-in inspector for a component that has no custom widgets
	template <typename T>
	class ReflectedInspector final : public IComponentInspector
	{
	public:
		[[nodiscard]] std::string_view Key() const override { return T::kKey; }

		void Draw(Entity e) override
		{
			if (T* value = e.try_get_mut<T>()) utils::DrawReflected(*value);
		}
	};
}
