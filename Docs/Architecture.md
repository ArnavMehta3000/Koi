# Architecture

Koi is split into three pieces: the engine, the editor, and your game. The one firm rule is that **the engine never knows the editor exists**, and the game never asks whether it's running in the editor.

```
<Game>.exe          = RuntimeMain + your game + Koi.Engine
<Game>-Editor.exe   = EditorMain  + your game + Koi.Editor + Koi.Engine
```

| Target | |
|---|---|
| `Koi.Engine` | Static lib. Every runtime system. No ImGui, no editor types |
| `Koi.Editor` | Static lib, `koi::editor`. Panels, inspectors, importers, cooking. Only linked in `editor-*` modes |
| `<Game>` | Your exe. The `KoiGame` rule picks the launcher and links what's needed |

## Boot

1. `Launcher/{Editor,Runtime}Main.cpp` calls `RunApplication`.
2. The VFS mounts `engine://` and `project://`. The editor mounts the source trees plus the compiled shaders. A standalone build mounts the cooked content.
3. The `Engine` singleton is created, then your `AppBase::Create` runs, then the project TOML is loaded and logging is configured.
4. In the editor, your app is wrapped in an `EditorApp`. Standalone, it runs directly.
5. `Engine::RunApp` runs the main loop.

## Engine

`Engine` owns the `Window` and three subsystems, created in this order and shut down in reverse: `InputManager`, `Renderer`, `AssetRegistry`. Get one with `Engine::Get().GetSubsystem<T>()`.

- **App host.** `IAppHost` is the seam between the engine and whatever it runs. `AppBase` implements it and owns the `World`. Your game subclasses `AppBase` and overrides its `On*` hooks.
- **ECS.** Koi has its own sparse-set ECS, owned by `World`. Components register through `ComponentRegistry`, and systems and observers through `OnRegisterSystems`. Entities have names and a parent/child hierarchy: `Transform` is local to the parent, and `WorldTransform` is rebuilt from it every frame. A camera is just an entity with `Transform` and `CameraComponent`.
- **Scenes.** Scenes are TOML files. The `Serializer<T>` framework writes components to and from TOML: reflect-cpp derives it from the struct, or you write a specialization by hand. See [Components](Components.md).
- **Rendering.** The `Renderer` owns the D3D12 device, swapchain and a render graph. The deferred pipeline runs its features in order: Shadow, GBuffer, Lighting, AutoExposure, Tonemap, Grid, DebugDraw, Blit. Features never reference each other; they pass data through the graph's blackboard. Resources are bindless. See [Rendering](Rendering.md).
- **Assets.** At runtime the engine only reads `.kmesh`, `.kmat` and `.ktex`. Each file has a UUID, and `AssetIndex` maps UUIDs to paths. Caches are refcounted and hand out handles. See [Assets](Assets.md).
- **Input.** Keyboard and mouse use raw input, and gamepads use GameInput. Game code reads named actions from `[input.actions]`, never raw keys. See [Input](Input.md).
- **Play state.** The engine tracks `Edit`, `Playing` or `Paused`. See [Play mode](PlayMode.md).

## Editor

`EditorApp` wraps your game's `AppBase` and draws the editor over the frame. It owns an `EditorContext`: a set of services looked up by type (`Selection`, `AssetDatabase`, `InspectorRegistry`, `PlaySession`, `PanelHost` and more), fetched with `ctx.Get<T>()`.

- **Panels** derive `editor::Panel` and save their own state to `.koi/Editor.toml`.
- **Commands** live in the `CommandRegistry`, which drives menus, shortcuts and the Ctrl+K palette.
- **Play** snapshots the world to TOML. **Stop** restores it.
- **Import** turns glTFs and images into Koi's binary formats on background jobs.
- **Cook** writes the content a standalone build needs. See [Packaging](Packaging.md).
- **F5** recompiles shaders that changed. See [Shaders](Shaders.md).

A game hooks into the editor from its `Editor/` folder with an `EditorExtension`. That's where it registers inspectors, panels and commands. See [Editor extensions](EditorExtensions.md), and [Customization](Customization.md) for everything a game can change.

## Where things live

```
Launcher/           the two main() files
Koi/Engine/         engine code, plus Content/ (engine shaders and meshes)
Koi/Editor/         editor code
Xmake/              modes, options, rules, package recipes
Plugins/Koi/        the `xmake koi` plugin and the project template
External/           vendored libraries
Projects/           games
```
