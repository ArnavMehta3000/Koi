# Components and serialization

Game code is C++ only. A component is a plain struct. Scenes save it as TOML, and the editor shows it in the inspector.

## A reflected component

```cpp
// Source/Components/Spin.h
#include "Koi/Engine/Runtime/ReflectSerializer.h"

struct Spin
{
	static constexpr std::string_view kKey = "spin";  // the scene table name and the inspector's ID
	static constexpr bool kReflect = true;            // reflect-cpp writes Serializer<Spin> for you

	koi::Vector3 Axis  = koi::Vector3(0.0f, 1.0f, 0.0f);
	koi::f32     Speed = 90.0f;
};
```

In a scene it looks like this. Keys are the field names in snake_case:

```toml
[[entity]]
name = 'Box'

    [entity.spin]
    axis = [ 0.0, 1.0, 0.0 ]
    speed = 90.0
```

A key missing from the file keeps the field's default value, and an unknown key logs a warning. To keep a field out of the file (runtime-only state), wrap it in `rfl::Skip<T>`. The inspector still shows it, greyed out.

## Registering it

Register components in `OnRegisterComponents` and systems in `OnRegisterSystems`:

```cpp
void MyGameApp::OnRegisterComponents()
{
	koi::ComponentRegistry::Get().Register<koi::ComponentType<Spin>>(50);  // 50 = order in the inspector
}

void MyGameApp::OnRegisterSystems()
{
	koi::SystemRegistry::Get().Register<SpinSystem>();
}
```

`ComponentType<T>(drawOrder, required)` handles adding, removing, loading and saving. A required component can't be removed, and is created with default values when a scene leaves it out.

The system runs for every entity that has the listed components:

```cpp
class SpinSystem final : public koi::System<koi::Transform, const Spin>
{
public:
	std::string_view Name() const override { return "Spin"; }
	void Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& xform, const Spin& spin) override;
};
```

Override `Phase()` to run in `PreUpdate`, `FixedUpdate`, `Update` (the default), `PostUpdate` or `PreRender`. Derive `TaskSystem` instead for a system that runs once per frame without looping over entities. If a system throws, the editor stops Play and restores the world; a shipped game exits with the error logged. See [Error handling](ErrorHandling.md).

Then give the component an inspector. See [Editor extensions](EditorExtensions.md). Systems only run during Play; see [Play mode](PlayMode.md).

## Writing a serializer by hand

Use this when the file format should differ from the struct. Leave out `kReflect` and specialize `koi::Serializer<T>`:

```cpp
template <>
struct koi::Serializer<Health>
{
	static koi::f64 Serialize(const Health& h) { return h.Max; }

	static std::optional<Health> Deserialize(const toml::node& node)
	{
		if (auto max = node.value<koi::f32>()) return Health{ .Max = *max, .Current = *max };
		return std::nullopt;
	}
};
```

`Serialize` returns either a TOML value (`bool`, integers, `f64`, `std::string`) or a `toml::table` / `toml::array`. `Deserialize` returns `std::nullopt` when the node isn't valid. Native types, `f32`, `Vector2/3/4` and `Quaternion` already have serializers, so they work as fields of either kind.

The engine's `MeshRef` and `MaterialRef` work this way: the struct holds a cache handle, while the file stores the asset's UUID.

`Serializer<T>` isn't limited to components. Editor panels use it to save their settings in `.koi/Editor.toml`.
