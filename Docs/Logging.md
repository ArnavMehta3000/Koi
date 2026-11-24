# Logging

Koi's logger wraps spdlog. Every message has a **category** and a **level**, and each category has its own level filter.

```cpp
KOI_LOG(Render, Info, "Created {} targets", count);
```

The format string uses `{fmt}` syntax (`{}`, `{:.2f}`, ...).

## Levels

Pick a level by who the message is for, whatever its category:

| Level | Meaning | Example |
|---|---|---|
| `Critical` | The process or the device can't continue | crash, device removed |
| `Error` | Something requested didn't happen, or its result is wrong | a scene or shader failed to load |
| `Warn` | It worked, but with a fallback or dropped data | missing material, unknown component table |
| `Info` | An outcome whoever is making the game cares about | scene saved, asset imported |
| `Debug` | What the engine did, once per operation | startup steps, swapchain resize |
| `Trace` | One item inside an operation | each texture, shader or VFS read |

A batch gets one `Debug` summary and its items are `Trace`. Asset imports are the exception: each finished import is `Info`.

By default `Debug` and up is recorded, and the Log panel shows `Info` and up. Its Verbose button reveals the `Debug` messages already recorded. `Trace` has to be turned on first, e.g. `--log RHI=Trace`.

## Your own categories

Declare a category once, at global scope, in a header your code shares:

```cpp
// Source/MyGameLog.h
#include "Koi/Engine/Logging/Logger.h"

KOI_LOG_CATEGORY(Gameplay);
KOI_LOG_CATEGORY(AI);
```

```cpp
KOI_LOG(Gameplay, Debug, "Player spawned at ({}, {})", pos.x, pos.z);
```

There's already a `Game` category for anything that doesn't need its own. Names have to be unique across the engine, the editor, the modules you use and your game, so `Render` or `Editor` won't compile as a game category. The editor's Log panel marks game categories so they stand out from the engine's.

## Choosing what gets logged

There are three filters. A message has to pass all of them:

1. **Compile time.** `xmake f --LogLevel=<0..4>` (0 = Trace ... 4 = Error). Calls below this level aren't compiled at all, so they cost nothing and can't be turned back on at runtime.
2. **The project file.** `[logging] level` is the default for every category. `[logging.categories]` overrides it per category:

   ```toml
   [logging]
   level = "Debug"

   [logging.categories]
   Gameplay = "Trace"
   RHI      = "Warn"
   ```

3. **The command line**, for a single run. `--log` overrides the file. `all=<level>` resets every category first:

   ```sh
   MyGame-Editor.exe --log all=Warn,Gameplay=Trace
   ```

An unknown level name in the TOML or on the command line logs a warning and is ignored.

## Where logs go

| `[logging]` key | |
|---|---|
| `file` | Log file path. A relative path goes under `<project>/.koi/` in the editor, or `<exe>/.koi/` standalone. Empty turns the file off |
| `debug_sink` | Also send messages to `OutputDebugString`, which shows up in the Visual Studio Output window |
| `console` | Open a console window. `game-debug` builds already have one |
| `history` | How many recent messages the editor's Log panel keeps. 0 turns the panel's history off |

The Log panel can filter by level, by category and by text, and shows each message's source file and line.

## Conventions the engine follows

- Don't log every frame. Use the profiler for that (`*-profile` modes, `ZoneScoped`).
- Always put braces around an `if` or loop body that contains `KOI_LOG`. The macro compiles to nothing in some configs.
