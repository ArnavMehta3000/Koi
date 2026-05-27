# Koi Engines

Koi is a work-in-progress DirectX 12 (bindless) game engine, built primarily for learning purposes. Built for Windows only & uses C++23.

## Cloning

```sh
git clone  https://github.com/your-username/Koi.git
```

## Building

Requires [xmake](https://xmake.io) 3.0.8 or newer.

```sh
xmake                        # build (default: debug)
xmake run koi-runtime        # build and run the Sandbox app
```

### Build configurations

| Mode | Description |
|---|---|
| `debug` | Full debug info, `KOI_DEBUG` defined |
| `release` | Optimized, no debug info |
| `releasedbg` | Optimized with debug info |
| `profile` | Release + Tracy profiling (`KOI_ENABLE_PROFILING`, `TRACY_ENABLE`) |

Switch modes with:

```sh
xmake f -m <mode>
xmake
```

### Options

| Option | Values | Description |
|---|---|---|
| `--LogLevel` | `0`–`4` | Compile-time log floor (0=trace, 1=debug, 2=info, 3=warn, 4=error) |

Example: `xmake f -m debug --LogLevel=0`

## Dependencies

Fetched automatically via xmake-repo:

| Package | Purpose |
|---|---|
| `glfw` | Window creation and input |
| `directx-headers` | D3D12 / DXGI headers |
| `dxc` | HLSL shader compiler |
| `hlslpp` | SIMD math library |
| `stb` | Image loading |
| `spdlog` | Logging |
| `toml++` | TOML config parsing |
| `cli11` | Command-line argument parsing |
| `fastgltf` | glTF 2.0 model loading |
| `tracy` | Frame profiler (active in `profile` mode) |

## Project file

The engine is configured at runtime via a TOML project file passed with `-p <file>` (e.g. `Sandbox/Sandbox.toml`). It describes the app name, window settings, asset directories, and shader registrations.
