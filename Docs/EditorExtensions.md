# Editor extensions

Your game's editor code lives in its `Editor/` folder, which only builds in `editor-*` modes. `Source/` must never include `Koi/Editor`, so this folder is the only place the game touches the editor.

## The extension

```cpp
// Editor/MyGameEditor.cpp
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"

namespace
{
	class MyGameEditor final : public koi::editor::EditorExtension
	{
	public:
		void OnStartup(koi::editor::EditorContext& ctx) override
		{
			// register inspectors, panels and commands here
		}

		void OnShutdown() override {}
	};
}

KOI_EDITOR_EXTENSION(MyGameEditor);
```

`KOI_EDITOR_EXTENSION` registers the class when the program starts. The editor creates it after your game's `OnInit`. You can have as many extensions as you like, in any files under `Editor/`.

`EditorContext` holds the editor's services. Get one with `ctx.Get<T>()`: `InspectorRegistry`, `PanelManager`, `CommandRegistry`, `Selection`, `AssetDatabase`, `PlaySession`, `UndoStack` and more. Keep the pointer if you need it later.

## Inspectors

Without an inspector, a component still shows up in the Inspector panel and can be removed, but you can't edit its fields. If plain widgets for each field are fine, use the reflected one:

```cpp
ctx.Get<koi::editor::InspectorRegistry>()->Emplace<koi::editor::ReflectedInspector<PlayerController>>();
```

It draws one row per field: a checkbox for `bool`, drag boxes for `f32`, `i32` and `Vector3`, a text box for `std::string`, and a combo box for enums. `koi::Transient<>` fields are greyed out. Field names become labels, so `MoveSpeed` shows as "Move Speed".

For custom widgets, derive `ComponentInspector<T>` and draw with ImGui:

```cpp
class SpinInspector final : public koi::editor::ComponentInspector<Spin>
{
public:
	void Draw(koi::Entity e) override
	{
		Spin* spin = e.try_get_mut<Spin>();
		if (!spin || !koi::editor::utils::BeginPropertyGrid("##spin")) return;

		koi::editor::utils::PropertyLabel("Axis");
		koi::editor::utils::DragAxes("##Axis", &spin->Axis.x, 3);
		koi::editor::utils::PropertyLabel("Speed");
		ImGui::DragFloat("##Speed", &spin->Speed, 1.0f, -1440.0f, 1440.0f, "%.1f deg/s");

		koi::editor::utils::EndPropertyGrid();
	}
};
```

Undo and redo work for both kinds with no extra code. Inside a custom inspector, `utils::DrawReflected(value)` draws a whole struct the default way, which is handy for a nested settings struct.

## Widgets

`Koi/Editor/UI/Widgets.h` has the widgets the built-in panels use, so your UI matches theirs:

| | |
|---|---|
| `BeginPropertyGrid` / `PropertyLabel` / `EndPropertyGrid` | Two-column rows with labels on the left. Give each widget a `##` label |
| `DragAxes` | Drag boxes for x/y/z with colored axes |
| `SectionHeader` | A collapsible header, optionally with a menu |
| `IconButton`, `SegmentedControl`, `SearchBar` | Toolbar controls |
| `AssetField` | A slot you can drop an asset onto |

Icons are Font Awesome 6 (`ICON_FA_*` from `IconsFontAwesome6.h`).

## Panels

```cpp
class StatsPanel final : public koi::editor::Panel
{
public:
	static constexpr koi::editor::PanelInfo kInfo{ .Name = "Stats", .Icon = ICON_FA_CHART_LINE };

	explicit StatsPanel(koi::editor::EditorContext* ctx) : m_selection(ctx->Get<koi::editor::Selection>()) {}

	void OnDraw() override { ImGui::TextUnformatted("Hello"); }

	void SaveState(toml::table& state) const override { state.insert("expanded", m_expanded); }
	void LoadState(const toml::table& state) override { m_expanded = state["expanded"].value_or(false); }

private:
	koi::editor::Selection* m_selection;
	bool                    m_expanded = false;
};

// in OnStartup
ctx.Get<koi::editor::PanelManager>()->Add<StatsPanel>();
```

The editor owns the window, and `OnDraw` only fills it in. `kInfo.Name` is the window title and the key the panel's state is saved under (`[panels.Stats]` in `.koi/Editor.toml`), so don't rename it. For quick debug UI, `PanelManager::Add("Name", icon, [] { ... })` takes just a draw function.

## Commands

Editor actions go through the `CommandRegistry` instead of checking keys by hand. Registered commands show up in the Ctrl+K palette and get their shortcut for free:

```cpp
ctx.Get<koi::editor::CommandRegistry>()->Register(
{
	.Id         = "mygame.respawn",
	.Label      = "Respawn Player",
	.Icon       = ICON_FA_ROTATE,
	.Shortcut   = ImGuiMod_Ctrl | ImGuiKey_R,
	.Execute    = [] { /* ... */ },
	.CanExecute = [] { return koi::Engine::Get().GetPlayState() != koi::PlayState::Edit; },
});
```

`Scope` limits the command to one panel while that panel has focus. `IsChecked` turns the command into a toggle.
