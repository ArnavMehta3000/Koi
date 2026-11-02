# Koi

Koi is a DirectX 12 game engine I'm writing to learn. It uses bindless resources and C++23, and it only runs on Windows.

You make a game in the editor (`<Game>-Editor.exe`). When you're done, you Cook the content and ship a standalone Release `<Game>.exe` that runs without the editor. See [Architecture](Docs/Architecture.md) for more info.

## Requirements

- Windows 10/11 with a D3D12 GPU that supports Shader Model 6.6
- MSVC / VS2026 (needs C++23 support)
- [xmake](https://xmake.io) 3.1.1+

Run xmake from PowerShell or cmd. Under Git Bash, xmake thinks the platform is mingw.

## Quick start

```sh
git clone https://github.com/ArnavMehta3000/Koi.git
cd Koi
xmake run Sandbox        # builds and launches Sandbox-Editor.exe
```

A plain `xmake` builds the engine and your own projects in `Projects/`. It skips the samples, so build those by name.
Run `GenerateProjectFiles.bat` to generate a Visual Studio solution.

## Build modes

The prefix decides what the game exe is. `editor-*` builds `<Game>-Editor.exe` with the editor linked in. `game-*` builds the standalone `<Game>.exe`.

| Mode | What you get |
|---|---|
| `editor-debug` | Editor, unoptimized. This is the default |
| `editor-releasedbg` | Editor, optimized |
| `editor-profile` | Editor, optimized, with Tracy |
| `game-debug` | Standalone, unoptimized, with a console |
| `game-release` | Standalone, optimized and stripped. This is what you ship |
| `game-profile` | Standalone, optimized, with Tracy |

```sh
xmake f -m game-release
xmake
```

Every mode writes a PDB and has its own build directory. In profile modes, `KOI_ENABLE_PROFILING` and `TRACY_ENABLE` are on, and `xmake koi tracy` opens the matching Tracy viewer.

### Options

| Option | Values | |
|---|---|---|
| `--LogLevel` | `0`–`4` | Compile-time log floor: 0 trace, 1 debug, 2 info, 3 warn, 4 error. Anything below it isn't compiled in |

```sh
xmake f -m editor-debug --LogLevel=2
```

## Command line

No exe needs arguments. The optional flags are `-p <project.toml>`, `--d3ddebug`, `--gpuvalidation` and `--log Cat=Level,...`. See [Command line](Docs/CommandLine.md) for what they do and how a game adds its own.

## Projects

A project is any folder `Samples/<Name>/` or `Projects/<Name>/` that holds a `<Name>.koi.toml`. Xmake finds it from that file. `Samples/` is tracked in this repo. `Projects/` is where I am testing a dummy Subway Surfers style game (meant to be gitignored)

```sh
xmake koi new MyGame     # scaffold Projects/MyGame from Plugins/Koi/Template
xmake run MyGame         # build and open the editor
```

The layout, the project file and where code goes are in [Starting a project](Docs/NewProject.md). To ship a standalone build, Cook in the editor and then `xmake install` from `game-release`. The steps are in [Shipping a game](Docs/Packaging.md).

## Docs

- [Architecture](Docs/Architecture.md): how the engine, the editor and your game fit together
- [Starting a project](Docs/NewProject.md): `xmake koi new`, the project layout and the project file
- [Components and serialization](Docs/Components.md): components, systems and `Serializer<T>`
- [Editor extensions](Docs/EditorExtensions.md): inspectors, panels, commands and ImGui widgets
- [Customization](Docs/Customization.md): everything a game can change
- [Assets](Docs/Assets.md): formats, UUIDs, importing and reimporting
- [Shaders](Docs/Shaders.md): `Shaders.json`, what the build makes, and F5
- [Rendering](Docs/Rendering.md): custom pipelines, features and render graph passes
- [Logging](Docs/Logging.md): categories, levels and where logs go
- [Input](Docs/Input.md): actions, every binding name, and the focus rule
- [Play mode](Docs/PlayMode.md): what runs when, and what survives Stop
- [Error handling](Docs/ErrorHandling.md): `Result<T>`, exceptions, asserts and crash dumps
- [Command line](Docs/CommandLine.md): flags, and adding your own
- [Shipping a game](Docs/Packaging.md): Cook and `xmake install`

## Assets and shaders

At runtime Koi only reads its own `.kmesh`, `.kmat` and `.ktex` files. You get them by dragging glTFs and images onto the editor, and files refer to each other by UUID. Shaders are HLSL listed in a `Shaders.json`, compiled by the build, and reloaded with F5. See [Assets](Docs/Assets.md) and [Shaders](Docs/Shaders.md).

## Xmake rules and plugins

| | |
|---|---|
| `KoiGame` | Turns a target into a game: adds the right launcher main, links `Koi.Editor` in editor modes, bakes the source paths into the exe, and handles `xmake install` |
| `KoiShaders` | Compiles a `Shaders.json` with dxc into `<autogendir>/Built/Shaders`, writing a `Manifest.toml` and a `.refl` per variant. Only rebuilds what changed |
| `KoiMode` | Applies the mode table in `Xmake/Modes.lua` |
| `Natvis` | Adds packages' `.natvis` files to the VS solution |
| `xmake koi` | `new`, `list`, `tracy` (`Plugins/Koi`) |


## Dependencies

Xmake fetches everything. Each package uses a recipe in `Xmake/Packages/` that was adapted from xmake-repo, and is pinned to a git commit in `Xmake/Packages.lua`. So a build is reproducible, and upgrading a package means changing its pin.

| Package | Used for |
|---|---|
| `directx-headers` | D3D12 / DXGI headers |
| `dxc` | Shader compiler (release binaries) |
| `Flecs` | ECS |
| `reflect-cpp` (+ `ctre`) | Component serialization and default inspectors |
| `toml++` | Project, scene and settings files |
| `spdlog` | Logging |
| `cli11` | Command-line parsing |
| `stduuid` | Asset IDs |
| `box3d` | Physics (just being wired up) |
| `tracy-profiler`, `tracy-tools` | Profiling (the tools only install in profile modes) |
| `directxtex` | Texture import and BC compression (editor only) |
| `fastgltf` (+ `simdjson`) | glTF import (editor only) |
| `stb` | Image loading (editor only) |

Kept in `External/` and built from source:

| Library | Used for |
|---|---|
| `SimpleMath` | Math, from DirectXTK, on top of DirectXMath |
| `GameInput` | Gamepad input and rumble |
| `ImGui` (docking) + `ImGuizmo` | Editor UI and gizmos |
| `IconFontCppHeaders` | Font Awesome icons in the editor |
| `KoiProfiler` | The editor's in-app profiler |

## License

See [LICENSE](LICENSE).
