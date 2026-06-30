# C++23 Module Migration Plan

## Design Decisions

### Module naming
`Koi.<Subsystem>.<Name>` — e.g., `Koi.Core.Types`, `Koi.Graphics.Camera`.

### File conventions
| Current | New | Purpose |
|---|---|---|
| `Foo.h` + `Foo.cpp` | `Foo.ixx` + `Foo.cpp` | Module interface unit + implementation unit |
| Macro-heavy `Foo.h` | `Foo.ixx` (class) + `Foo.h` (macros only) | Companion-header pattern |

### Companion-header pattern (for macro-only files)
Macros cannot cross module boundaries. For every header that defines macros users need (`KOI_LOG_*`, `KOI_MEM_TAG_SCOPE`, `KOI_ENABLE_ENUM_FLAGS`, `DX_CHECK`, `KOI_IMPLEMENT_MAIN`), we **keep the `.h` file for macros only** and put the class/types in the `.ixx`. Files that use those macros must:
```cpp
import Koi.Core.Logger;   // (1) get the Logger class from the module
#include "Core/Logger.h"  // (2) get KOI_LOG_* macros from companion header
```

### Global module fragment pattern (for third-party deps)
Third-party headers go in the global module fragment of the `.ixx` file (before `export module`):
```cpp
module;                           // begin global module fragment
#include <spdlog/spdlog.h>
#include "Platform/Win32.h"
export module Koi.Core.Logger;    // end global module fragment, begin module
import Koi.Core.Types;            // import engine dependencies
export namespace koi { ... }
```

---

## Tier 1 — Files that STAY as headers (pure macro or special)
These do NOT get a `.ixx`. They continue to be `#include`d (in global module fragments or by code that needs the macros).

| File | Reason |
|---|---|
| `Core/Config.h` | Generated, defines `KOI_DEBUG` / `KOI_RELEASE` / `KOI_PROFILING` |
| `Core/Macros.h` | Pure utility macros: `KOI_BIT`, `KOI_STRINGIFY` |
| `Core/Profiling.h` | Tracy macro wrapper (`ZoneScoped`, `FrameMark`) |
| `Core/Core.h` | Aggregates Config + Types macros + CRT debug macros |
| `Platform/Win32.h` | Wraps `<windows.h>` + undef collisions |
| `Runtime/EntryPoint.h` | Defines `KOI_IMPLEMENT_MAIN` — used once in Launcher |

---

## Tier 2 — Files that get BOTH a `.ixx` and a companion `.h` (class + macros)

| Current `.h` | Module name | What stays in `.h` |
|---|---|---|
| `Core/Types.h` | `Koi.Core.Types` | `KOI_FILE`, `KOI_FILENAME` macros |
| `Core/Logger.h` | `Koi.Core.Logger` | `KOI_LOG_IMPL`, `KOI_LOG_TRACE` … `KOI_LOG_CRITICAL` |
| `Core/EnumFlags.h` | `Koi.Core.EnumFlags` | `KOI_ENABLE_ENUM_FLAGS`, `KOI_ENUM_FLAG_NAMES`, `KOI_FLAG_ENTRY` |
| `Memory/MemoryTracker.h` | `Koi.Memory.MemoryTracker` | `KOI_MEM_TAG_SCOPE` |
| `Graphics/RHI/DX12Common.h` | `Koi.Graphics.DX12Common` | `DX_CHECK` |

---

## Tier 3 — Files that convert cleanly to modules (no macros)
Listed in bottom-up dependency order (each module may only import modules listed before it).

### Core
| Current `.h` | Module name | Third-party in GMF |
|---|---|---|
| `Core/LogLevel.h` | `Koi.Core.LogLevel` | `Generated/Engine/LogLevel.h` |
| `Core/Object.h` | `Koi.Core.Object` | — |
| `Core/Handle.h` | `Koi.Core.Handle` | `<cassert>` |
| `Core/Patterns/Match.h` | `Koi.Core.Match` | — |
| `Core/StringUtils.h` | `Koi.Core.StringUtils` | `<string>`, `<algorithm>` |
| `Core/VirtualFileSystem.h` | `Koi.Core.VirtualFileSystem` | `<filesystem>` |
| `Core/MathTypes.h` | `Koi.Core.MathTypes` | `<hlslpp/...>` |
| `Core/Math.h` | `Koi.Core.Math` | `<hlslpp/...>` |
| `Core/Transform.h` | `Koi.Core.Transform` | — |

### Memory
| Current `.h` | Module name | Notes |
|---|---|---|
| `Memory/MemoryTags.h` | `Koi.Memory.MemoryTags` | — |
| `Memory/MemoryDebug.h` | `Koi.Memory.MemoryDebug` | — |

### Platform
| Current `.h` | Module name | Third-party in GMF |
|---|---|---|
| `Platform/InputCodes.h` | `Koi.Platform.InputCodes` | — |
| `Platform/Window.h` | `Koi.Platform.Window` | `Platform/Win32.h`, GLFW |
| `Platform/InputManager.h` | `Koi.Platform.InputManager` | — |

### Graphics RHI
| Current `.h` | Module name | Third-party in GMF |
|---|---|---|
| `Graphics/Format.h` | `Koi.Graphics.Format` | `<dxgiformat.h>` |
| `Graphics/Types.h` | `Koi.Graphics.Types` | — |
| `Graphics/RHI/PipelineDesc.h` | `Koi.Graphics.RHI.PipelineDesc` | DX12 headers |
| `Graphics/RHI/Resources.h` | `Koi.Graphics.RHI.Resources` | DX12 headers |
| `Graphics/RHI/DescriptorHeap.h` | `Koi.Graphics.RHI.DescriptorHeap` | — |
| `Graphics/RHI/CommandAllocator.h` | `Koi.Graphics.RHI.CommandAllocator` | — |
| `Graphics/RHI/CommandQueue.h` | `Koi.Graphics.RHI.CommandQueue` | — |
| `Graphics/RHI/CommandList.h` | `Koi.Graphics.RHI.CommandList` | — |
| `Graphics/RHI/Device.h` | `Koi.Graphics.RHI.Device` | DX12 headers |

### Graphics high-level
| Current `.h` | Module name |
|---|---|
| `Graphics/GPUMesh.h` | `Koi.Graphics.GPUMesh` |
| `Graphics/Camera.h` | `Koi.Graphics.Camera` |
| `Graphics/CameraController.h` | `Koi.Graphics.CameraController` |
| `Graphics/ShaderCompiler.h` | `Koi.Graphics.ShaderCompiler` |
| `Graphics/ShaderLoader.h` | `Koi.Graphics.ShaderLoader` |
| `Graphics/MeshLoader.h` | `Koi.Graphics.MeshLoader` |
| `Graphics/RenderPasses/RenderPass.h` | `Koi.Graphics.RenderPasses.RenderPass` |
| `Graphics/RenderPasses/TonemapPass.h` | `Koi.Graphics.RenderPasses.TonemapPass` |
| `Graphics/RenderPasses/ScenePass.h` | `Koi.Graphics.RenderPasses.ScenePass` |
| `Graphics/RenderWindow.h` | `Koi.Graphics.RenderWindow` |
| `Graphics/RendererResources.h` | `Koi.Graphics.RendererResources` |
| `Graphics/Renderer.h` | `Koi.Graphics.Renderer` |

### Scene & Runtime
| Current `.h` | Module name |
|---|---|
| `Scene/Scene.h` | `Koi.Scene.Scene` |
| `Runtime/Timer.h` | `Koi.Runtime.Timer` |
| `Runtime/CommandLine.h` | `Koi.Runtime.CommandLine` |
| `Runtime/AppBase.h` | `Koi.Runtime.AppBase` |
| `Runtime/EngineHooks.h` | `Koi.Runtime.EngineHooks` |
| `Runtime/Engine.h` | `Koi.Runtime.Engine` |

---

## xmake.lua Changes

### `Engine/xmake.lua`
```diff
-  add_headerfiles("**.h", { public = true })
-  add_files("**.cpp", { public = true })
+  add_files("**.ixx", "**.cpp", { public = true })
+  add_headerfiles("**.h", { public = true })  -- keep for companion macro headers
```

### `Sandbox/xmake.lua`
```diff
-  add_files("Scripts/**.cpp")
-  add_headerfiles("Scripts/**.h")
+  add_files("Scripts/**.ixx", "Scripts/**.cpp")
+  add_headerfiles("Scripts/**.h")
```

---

## Implementation Order

Execute in strict bottom-up order to avoid forward-reference issues at build time:

1. `[MODIFY]` `Engine/xmake.lua` — add `**.ixx`
2. `[NEW]` `Engine/Core/Types.ixx` + `[MODIFY]` `Engine/Core/Types.h` (macros only)
3. `[NEW]` `Engine/Core/LogLevel.ixx`
4. `[NEW]` `Engine/Core/Object.ixx`
5. `[NEW]` `Engine/Core/Handle.ixx`
6. `[NEW]` `Engine/Core/Match.ixx`
7. `[NEW]` `Engine/Core/StringUtils.ixx` + `[MODIFY]` `Engine/Core/StringUtils.cpp`
8. `[NEW]` `Engine/Core/VirtualFileSystem.ixx` + `[MODIFY]` `Engine/Core/VirtualFileSystem.cpp`
9. `[NEW]` `Engine/Core/MathTypes.ixx`
10. `[NEW]` `Engine/Core/Math.ixx`
11. `[NEW]` `Engine/Core/Transform.ixx` + `[MODIFY]` `Engine/Core/Transform.cpp`
12. `[NEW]` `Engine/Core/Logger.ixx` + `[MODIFY]` `Engine/Core/Logger.h` (macros only) + `[MODIFY]` `Engine/Core/Logger.cpp`
13. `[NEW]` `Engine/Core/EnumFlags.ixx` + `[MODIFY]` `Engine/Core/EnumFlags.h` (macros only)
14. `[NEW]` `Engine/Memory/MemoryTags.ixx`
15. `[NEW]` `Engine/Memory/MemoryDebug.ixx` (if applicable)
16. `[NEW]` `Engine/Memory/MemoryTracker.ixx` + `[MODIFY]` `Engine/Memory/MemoryTracker.h` (macros only)
17. `[MODIFY]` `Engine/Memory/MemoryOverrides.cpp` — import modules
18. `[NEW]` `Engine/Platform/InputCodes.ixx`
19. `[NEW]` `Engine/Platform/Window.ixx` + `[MODIFY]` `Engine/Platform/Window.cpp`
20. `[NEW]` `Engine/Platform/InputManager.ixx` + `[MODIFY]` `Engine/Platform/InputManager.cpp`
21. `[NEW]` `Engine/Graphics/Format.ixx`
22. `[NEW]` `Engine/Graphics/Types.ixx`
23. `[NEW]` `Engine/Graphics/RHI/DX12Common.ixx` + `[MODIFY]` `Engine/Graphics/RHI/DX12Common.h` (macros only)
24. `[NEW]` `Engine/Graphics/RHI/PipelineDesc.ixx`
25. `[NEW]` `Engine/Graphics/RHI/Resources.ixx`
26. `[NEW]` `Engine/Graphics/RHI/DescriptorHeap.ixx` + `[MODIFY]` `Engine/Graphics/RHI/DescriptorHeap.cpp`
27. `[NEW]` `Engine/Graphics/RHI/CommandAllocator.ixx` + `[MODIFY]` `Engine/Graphics/RHI/CommandAllocator.cpp`
28. `[NEW]` `Engine/Graphics/RHI/CommandQueue.ixx` + `[MODIFY]` `Engine/Graphics/RHI/CommandQueue.cpp`
29. `[NEW]` `Engine/Graphics/RHI/CommandList.ixx` + `[MODIFY]` `Engine/Graphics/RHI/CommandList.cpp`
30. `[NEW]` `Engine/Graphics/RHI/Device.ixx` + `[MODIFY]` `Engine/Graphics/RHI/Device.cpp`
31. `[NEW]` `Engine/Graphics/GPUMesh.ixx` + `[MODIFY]` `Engine/Graphics/GPUMesh.cpp`
32. `[NEW]` `Engine/Graphics/Camera.ixx` + `[MODIFY]` `Engine/Graphics/Camera.cpp`
33. `[NEW]` `Engine/Graphics/CameraController.ixx` + `[MODIFY]` `Engine/Graphics/CameraController.cpp`
34. `[NEW]` `Engine/Graphics/ShaderCompiler.ixx` + `[MODIFY]` `Engine/Graphics/ShaderCompiler.cpp`
35. `[NEW]` `Engine/Graphics/ShaderLoader.ixx` + `[MODIFY]` `Engine/Graphics/ShaderLoader.cpp`
36. `[NEW]` `Engine/Graphics/MeshLoader.ixx` + `[MODIFY]` `Engine/Graphics/MeshLoader.cpp`
37. `[NEW]` `Engine/Graphics/RenderPasses/RenderPass.ixx` + `[MODIFY]` `Engine/Graphics/RenderPasses/RenderPass.cpp`
38. `[NEW]` `Engine/Graphics/RenderPasses/TonemapPass.ixx` + `[MODIFY]` `Engine/Graphics/RenderPasses/TonemapPass.cpp`
39. `[NEW]` `Engine/Graphics/RenderPasses/ScenePass.ixx` + `[MODIFY]` `Engine/Graphics/RenderPasses/ScenePass.cpp`
40. `[NEW]` `Engine/Graphics/RenderWindow.ixx` + `[MODIFY]` `Engine/Graphics/RenderWindow.cpp`
41. `[NEW]` `Engine/Graphics/RendererResources.ixx` + `[MODIFY]` `Engine/Graphics/RendererResources.cpp`
42. `[NEW]` `Engine/Graphics/Renderer.ixx` + `[MODIFY]` `Engine/Graphics/Renderer.cpp`
43. `[NEW]` `Engine/Scene/Scene.ixx` + `[MODIFY]` `Engine/Scene/Scene.cpp`
44. `[NEW]` `Engine/Runtime/Timer.ixx` + `[MODIFY]` `Engine/Runtime/Timer.cpp`
45. `[NEW]` `Engine/Runtime/CommandLine.ixx` + `[MODIFY]` `Engine/Runtime/CommandLine.cpp`
46. `[NEW]` `Engine/Runtime/AppBase.ixx` + `[MODIFY]` `Engine/Runtime/AppBase.cpp`
47. `[NEW]` `Engine/Runtime/EngineHooks.ixx`
48. `[NEW]` `Engine/Runtime/Engine.ixx` + `[MODIFY]` `Engine/Runtime/Engine.cpp`
49. `[MODIFY]` `Engine/Runtime/RunApplication.cpp` — import modules
50. `[MODIFY]` `Sandbox/Scripts/SandboxApp.h` → `SandboxApp.ixx` + `SandboxApp.cpp`
51. `[MODIFY]` `Sandbox/xmake.lua`

---

## Verification
After each group of changes:
```powershell
xmake         # should have 0 errors
xmake run     # should launch
```
