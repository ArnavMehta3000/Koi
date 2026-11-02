# Shipping a game

A shipped game is a Release `<Game>.exe` next to its cooked content. It has no editor and doesn't need the source tree.

## 1. Cook

In the editor, run **Cook** (File menu, or search the Ctrl+K palette). It writes to `<project>/.koi/Cooked/`:

```
.koi/Cooked/
  Engine/    engine shaders, meshes and materials
  Project/   your Content/, compiled shaders, Project.koi.toml
```

Cooking strips editor-only data out of `.kmesh`, `.kmat` and `.ktex` files, and leaves source files behind (glTF, images, `.hlsl`). It warns you if a shader is stale or missing from the build. Fix those first, because the cooked game only has the shaders the build compiled.

Cook again whenever your content changes. The exe doesn't care about content.

## 2. Build and install

```sh
xmake f -m game-release
xmake install MyGame
```

This copies the exe, its PDB and everything in `.koi/Cooked` into `<project>/Packaged/`:

```
Packaged/
  MyGame.exe
  MyGame.pdb
  Engine/
  Project/
```

Zip that folder and you're done. `xmake uninstall MyGame` deletes it.

Installing from an `editor-*` mode doesn't package anything and prints a reminder. If nothing has been cooked yet, it stops with an error.

## Testing before you install

`xmake f -m game-debug && xmake run MyGame` runs the standalone exe from `.koi/Cooked`. You get a console window and full debug info. Use `game-profile` to profile with Tracy (`xmake koi tracy` opens the viewer).

At runtime, logs go to `<exe>/.koi/Logs` and crash dumps to `<exe>/.koi/Crashes`.
