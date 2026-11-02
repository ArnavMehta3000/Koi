# Starting a project

```sh
xmake koi new MyGame     # creates Projects/MyGame
xmake run MyGame         # builds and opens MyGame-Editor.exe
```

The name must be a valid identifier (letters, digits, underscores) and can't match an existing project. `xmake koi list` shows every project xmake can find.

## How projects are found

A project is any `Samples/<Name>/` or `Projects/<Name>/` folder that holds a `<Name>.koi.toml`, where the folder name and the file name match. The root `xmake.lua` finds those files and includes each project's `xmake.lua`. The engine never refers to a game by name.

`Projects/` is gitignored, so your game can be its own repo inside it. A plain `xmake` builds `Projects/` but skips `Samples/`. Build a sample by name: `xmake build Sandbox`.

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

`AppBase` has hooks to override: `OnRegisterComponents`, `OnRegisterSystems`, `OnInit`, `OnTick`, `OnRender`, `OnBeginPlay`, `OnEndPlay` and `OnShutdown`. Register components and systems in their own hooks, not in `OnInit`. [Components](Components.md) walks through a full component: the struct, its registration and its system. `Samples/Sandbox` has working examples, and [Customization](Customization.md) lists everything else a game can change.

## The project file

| Table | |
|---|---|
| `[project]` | `name`, `version`, `author` |
| `[window]` | `title`, `width`, `height`, `vsync` |
| `[logging]` | `file`, `level`, `history`, `console`, `debug_sink` (see [Logging](Logging.md)) |
| `[logging.categories]` | Per-category levels, e.g. `RHI = "Info"` |
| `[scenes]` | `startup` scene name, plus `[scenes.list]` mapping names to `project://...scene.toml` |
| `[input]` | `stick_deadzone`, `trigger_deadzone` |
| `[input.actions.<Name>]` | `negative` / `positive` keys, `buttons`, analog `axes` (`"-Gamepad.LeftY"` inverts it) |

Every action is one float in [-1, 1]. See [Input](Input.md) for how bindings combine and every key name. The editor's Project Settings panel edits most of this file.

## Shaders

To add your own shaders, create `Content/Shaders/Shaders.json` and list your `.hlsl` files in it. See [Shaders](Shaders.md).
