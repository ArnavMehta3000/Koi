# Starting a project

```sh
xmake koi new MyGame     # creates Projects/MyGame
xmake run MyGame         # builds and opens MyGame-Editor.exe
```

The name must be a valid identifier (letters, digits, underscores) and can't match an existing project. `xmake koi list` shows every project xmake can find.

## How projects are found

A project is any `Projects/<Name>/` folder that holds a `<Name>.koi.toml`, where the folder name and the file name match. The root `xmake.lua` finds those files and includes each project's `xmake.lua`. The engine never refers to a game by name.

A plain `xmake` builds every project. `Sandbox` is where engine features are tried out, one scene each; make a new project only for a separate game or something that needs its own setup. Build one by name: `xmake build Sandbox`.

## What you get

The scaffold comes from `Plugins/Koi/Template`, with `{{Name}}` replaced by your project name:

```
Projects/MyGame/
  MyGame.koi.toml               project settings
  xmake.lua                     target("MyGame") + add_rules("KoiGame")
  Source/MyGameApp.{h,cpp}      your AppBase subclass and AppBase::Create
  Editor/MyGameEditor.cpp       an EditorExtension, built only in editor-* modes
  Content/Scenes/Main.scene.toml   floor, box, camera and sun
```

The first build adds `.koi/` (logs, crash dumps, editor layout, cooked output). `xmake install` adds `Packaged/`. Both are generated, so keep them out of source control.

## Where code goes

- `Source/` builds in every mode. It must never include `Koi/Editor` or check whether it's running in the editor.
- `Editor/` builds only in `editor-*` modes. Inspectors and panels go here and register through `KOI_EDITOR_EXTENSION`. See [Editor extensions](EditorExtensions.md).
- `Content/` is mounted at `project://Content/...`. Drop glTFs and images onto the editor to import them. See [Assets](Assets.md).

`AppBase` has hooks to override: `OnRegisterComponents`, `OnRegisterSystems`, `OnRegisterModules`, `OnInit`, `OnSceneLoaded`, `OnTick`, `OnRender`, `OnBeginPlay`, `OnEndPlay` and `OnShutdown`. Register components, systems and modules in their own hooks, not in `OnInit`: the world is built right after them. [Components](Components.md) walks through a full component: the struct, its registration and its system. `Projects/Sandbox` has working examples, and [Customization](Customization.md) lists everything else a game can change.

## The project file

| Table | |
|---|---|
| `[project]` | `name`, `version`, `author` |
| `[window]` | `title`, `mode` (`windowed` / `borderless` / `fullscreen`; standalone only, the editor stays windowed), `width`, `height`, `vsync` |
| `[logging]` | `file`, `level`, `history`, `console`, `debug_sink` (see [Logging](Logging.md)) |
| `[logging.categories]` | Per-category levels, e.g. `RHI = "Info"` |
| `[scenes]` | `startup` scene name, plus `[scenes.list]` mapping names to `project://...scene.toml` |
| `[physics]` | `fixed_step`, seconds per `FixedUpdate` step (default 1/60) |
| `[input]` | `stick_deadzone`, `trigger_deadzone` |
| `[input.actions.<Name>]` | `negative` / `positive` keys, `buttons`, analog `axes` (`"-Gamepad.LeftY"` inverts it) |

Every action is one float in [-1, 1]. See [Input](Input.md) for how bindings combine and every key name.

## Modules

Optional engine features are modules, and a project only gets the ones it asks for. To add physics:

```lua
-- xmake.lua
add_deps("Koi.Module.Physics")
```

```cpp
// Source/MyGameApp.h
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Module.h"
#include "Koi/Modules/Physics/Source/PhysicsModule.h"

class MyGameApp : public koi::AppBase
{
public:
	using Modules = koi::ModuleList<koi::PhysicsModule>;

	void OnRegisterModules() override { Modules::Register(); }
};
```

```cpp
// Editor/MyGameEditor.cpp
#include "MyGameApp.h"
#include "Koi/Editor/Core/EditorModules.h"
#include "Koi/Modules/Physics/Editor/PhysicsEditor.h"

// in OnStartup
koi::editor::EditorModules<MyGameApp::Modules>::Register(ctx);
```

Modules register after the game's own systems, so your fixed-step systems run before physics unless you order them otherwise. Physics adds the `RigidBody`, `Collider` and `PhysicsSettings` components and the `SensorBegin` / `SensorEnd` events. `[physics] fixed_step` applies to every `FixedUpdate` system, with or without the module.

## Shaders

To add your own shaders, create `Content/Shaders/Shaders.json` and list your `.hlsl` files in it. See [Shaders](Shaders.md).
