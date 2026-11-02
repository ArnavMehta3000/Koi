# Shaders

Shaders are HLSL. A shader is only built if it's listed in its folder's `Shaders.json`:

- the engine's: `Koi/Engine/Content/Shaders/Shaders.json`
- your project's: `<project>/Content/Shaders/Shaders.json`. `KoiGame` picks this file up as soon as it exists.

```json
{
	"shaders": [
		{ "source": "OutlineVS.hlsl" },
		{ "source": "OutlinePS.hlsl", "defines": ["THICKNESS=2"] },
		{
			"source": "BlurCS.hlsl",
			"permutations": { "HORIZONTAL": [false, true], "TAPS": [5, 9] }
		}
	]
}
```

## The rules

- **The stage comes from the file name.** It must end in `VS`, `PS`, `CS`, `HS`, `DS` or `GS`. The entry point is `<Stage>Main` (`VSMain`, `PSMain`, ...) and the target is Shader Model 6.6.
- **`defines`** apply to that one shader.
- **`permutations`** build one variant for every combination of values. The example above makes 2 × 2 = 4 variants of `BlurCS`. Booleans become `0`/`1`.
- **Includes** are searched in the shader's own folder, the project root and `Koi/Engine`, so `#include "Content/Shaders/Interop/Interop.hlsli"` works from any project.
- **Debug builds** compile with `-Zi` and write a PDB. Other modes compile with `-O3`. Warnings are errors in every mode.

The rule's comment in `Xmake/ShaderRule.lua` also mentions `entry`, `model`, `exclude`, `args` and file-wide `defines`/`includes`. Those are **planned, not implemented**, and are ignored for now.

## What the build makes

The `KoiShaders` rule runs dxc during the normal build and writes to `<target autogendir>/Built/Shaders/`:

- a `.cso` per variant, plus a `.pdb` in debug modes
- a `.refl` per variant (reflection data; the engine reads `[numthreads]` for compute shaders from it)
- `Manifest.toml`, listing each variant's source, defines, outputs and every file it includes

It only recompiles a variant when its source, an included file or its flags changed. Outputs for shaders you remove from `Shaders.json` are deleted, so they don't end up in a cooked build by mistake.

## Using a shader

Refer to a shader by its virtual path without the extension:

```cpp
constexpr koi::ComputePipeline kBlur{ .ComputeShader = "project://Content/Shaders/BlurCS" };
```

**Pipelines can't pick a permutation yet.** `RasterPipeline` / `ComputePipeline` only know the path, so a shader used through them must have no `permutations`. Variants are only reachable through `ShaderCache::Get().GetShader(path, defines)` for now.

See [Rendering](Rendering.md) for root constants, the shared Interop structs and binding.

## F5 in the editor

The editor compiles too. **F5** recompiles every shader whose source or includes are newer than its `.cso`, then rebuilds the pipelines that use it. **Shift+F5** recompiles everything. If a shader fails to compile, the errors are logged and any pass using it draws nothing until you fix it.

Shaders that F5 compiled aren't seen by Cook; Cook only takes what the xmake build produced. It warns if a built shader is stale or missing, so **build before you Cook**. A shader that F5 compiles but `Shaders.json` doesn't list works in the editor and is missing from the shipped game.
