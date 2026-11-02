# Koi Engine

Koi is a work-in-progress DirectX 12 (bindless) game engine, built primarily for learning purposes. Built for Windows only & uses C++23.

## Cloning

```sh
git clone  https://github.com/your-username/Koi.git
```

## Building

Requires [xmake](https://xmake.io) 3.0.8 or newer.

```sh
xmake                        # build (default: editor-debug)
xmake run koi-runtime        # build and run the Sandbox app
```

### Build configurations

Each mode is a build flavour plus what the game exe is: `editor-*` builds `<Game>-Editor.exe`, `game-*` the standalone `<Game>.exe`.

| Mode | Description |
|---|---|
| `editor-debug` | Editor, unoptimized (default) |
| `editor-releasedbg` | Editor, optimized |
| `editor-profile` | Editor, optimized + Tracy profiling (`KOI_ENABLE_PROFILING`) |
| `game-debug` | Standalone game, unoptimized |
| `game-release` | Standalone game, optimized; what `xmake install` packages |
| `game-profile` | Standalone game, optimized + Tracy profiling |

All modes keep a PDB.

Switch modes with:

```sh
xmake f -m <mode>
xmake
```

### Options

| Option | Values | Description |
|---|---|---|
| `--LogLevel` | `0`–`4` | Compile-time log floor (0=trace, 1=debug, 2=info, 3=warn, 4=error) |

Example: `xmake f -m editor-debug --LogLevel=0`

## Dependencies

Fetched automatically via xmake-repo:

| Package | Purpose |
|---|---|
| `directx-headers` | D3D12 / DXGI headers |
| `dxc` | HLSL shader compiler |
| `stb` | Image loading |
| `spdlog` | Logging |
| `toml++` | TOML config parsing |
| `cli11` | Command-line argument parsing |
| `fastgltf` | glTF 2.0 model loading |
| `tracy` | Frame profiler (active in `profile` mode) |

Vendored in `External/` and built from source:

| Library | Purpose |
|---|---|
| `SimpleMath` | Math types (DirectX Toolkit, over DirectXMath) |
| `ImGui` | Editor UI (docking branch, Win32 + DX12 backends) |

## Project file

The engine is configured at runtime via a TOML project file passed with `-p <file>` (e.g. `Sandbox/Sandbox.toml`). It describes the app name, window settings, asset directories, and shader registrations.
