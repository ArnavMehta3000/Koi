# Assets

At runtime, Koi only reads its own binary formats:

| File | Holds |
|---|---|
| `.kmesh` | One interleaved vertex stream, indices, submeshes, and a material UUID per slot |
| `.kmat` | Named material parameters and texture UUIDs |
| `.ktex` | Texture data with mips, usually BC-compressed |

Each one starts with a 64-byte header holding the asset's **UUID**, followed by tagged chunks. Some chunks are marked editor-only (import settings, the source path) and are removed by Cook.

## IDs, not paths

Files refer to assets by UUID, never by path:

```toml
[entity.mesh]
id = '8e985361-92e3-5bd0-89b0-eabe0e977013'  # engine Box
```

When the engine starts, it reads the header of every asset under each `Content/` folder and builds a UUID → path index. That means you can move or rename an asset file and nothing breaks.

Code is different: it uses virtual paths, which are easier to read.

```cpp
GetWorld().Spawn().WithMesh("engine://Content/Meshes/Sphere.kmesh").At({ 0, 1, 0 });
```

Things to know:

- **Don't copy an asset file in Explorer.** The copy keeps the same UUID, so the index keeps one of them and warns about the other. Import again instead.
- Importing over an existing file **keeps its UUID**, so anything that refers to it keeps working.
- The engine's primitives (`Box`, `Capsule`, `Cone`, `Cylinder`, `Plane`, `Sphere`, `Torus` in `engine://Content/Meshes/`) get their IDs from their names: `utils::UUIDFromName("Koi.Engine.Mesh.Box")`. So their IDs never change.
- A missing or broken asset doesn't crash anything. It logs an error and you get a fallback mesh, material or texture.

## Importing

Drag a glTF (`.gltf`, `.glb`) or an image (`.png`, `.jpg`, `.tga`, `.bmp`, `.tif`, `.gif`, `.dds`, `.hdr`) onto the editor. The Import panel lists each file with its settings. Imports run in the background and report how long each step took.

**Models** go into a folder named after the model, with one `.kmesh`, a `.kmat` per material and a `.ktex` per texture. Settings: `Scale`, and whether to import materials. If materials are off, every slot uses the default material.

**Textures** are mipmapped and compressed according to their kind:

| Kind | Format | Guessed from the file name |
|---|---|---|
| Color | BC7 sRGB | the default |
| Linear | BC7 | `rough`, `metal`, `_orm`, `_arm`, `_ao`, `occlusion`, `mask`, `spec`, `height`, `disp` |
| NormalMap | BC5 (z is rebuilt in the shader) | `normal`, `_nrm`, `_nor`, ending in `_n` |
| Grayscale | BC4 | never guessed; pick it by hand |
| HDR | BC6H | `.hdr` files |

BC7 and BC6H are compressed on the GPU. BC5 and BC4, and any machine without a usable GPU, use the CPU. Turning compression off stores plain RGBA8 / R8 / RGBA16F instead.

## Reimporting

Every imported file remembers its source path and settings. **Reimport** in the asset browser runs the import again with the same settings and keeps the UUID. It needs the source file to still exist. If you delete the glTF or image, you can't reimport that asset anymore (Sandbox's Sponza and DamagedHelmet are like this).

## Source files and Cook

You can keep source files (glTF, images) in `Content/` next to what they produced. The asset browser hides them and Cook leaves them out. Whether to commit them is up to you: committing only the imported files keeps the repo small, but then you can't reimport.

See [Shipping a game](Packaging.md) for what Cook does with the rest.
