# Error handling

Koi uses each tool for one kind of failure:

| | Use it for | What happens |
|---|---|---|
| `Result<T>` | Failures the caller should handle: a missing file, bad data | The caller checks it |
| Exceptions | Failures that should stop the current operation | Caught at a boundary, logged with a stack trace |
| `KOI_ASSERT` | Bugs: things that should never happen | Breaks into the debugger, or aborts |

## Result

`Result<T>` is `std::expected<T, koi::Error>`, and `Error` is just a message:

```cpp
koi::Result<Level> LoadLevel(std::string_view path)
{
	auto file = koi::AssetFile::Load(path);
	if (!file) return koi::Error::Make("Couldn't open {}: {}", path, file.error().Message);
	// ...
	return level;
}

if (auto level = LoadLevel("project://Content/Levels/One.klevel"))
	Use(*level);
else
	KOI_LOG(Gameplay, Error, "{}", level.error().Message);
```

Use `Result<void>` for operations that return nothing. Engine APIs that can fail this way include `World::Load`, `World::Save` and `AssetFile::Load`. Mark your own with `[[nodiscard]]` so a result can't be ignored by accident.

## Exceptions

Throw `koi::Exception` (or any `std::exception`). The engine records the stack at the moment of the throw, so the log shows where the error happened, not just where it was caught.

Where exceptions are caught:

- **Systems** run under a guard. If one throws, the remaining systems are skipped for that frame and the error is passed up as a `koi::Exception`.
- **In the editor**, an exception during Play stops Play, logs the error and shows a toast. See [Play mode](PlayMode.md).
- **At the top level**, `RunApplication` catches anything left, logs it as `Critical` with the trace, and exits with a non-zero code. In a shipped game, that's what an uncaught exception from a system or `OnTick` does.

D3D12 calls in the engine go through `DX_CHECK(hr)`, which throws a `GfxError` (with the `HRESULT`) when a call fails.

## Asserts

```cpp
KOI_ASSERT(index < m_items.size(), "index {} out of range", index);
```

Asserts only exist in `*-debug` builds. A failed assert logs the condition, message, file, line and a stack trace. It breaks into the debugger if one is attached, and aborts otherwise. In every other mode the condition **isn't evaluated**, so don't put work that has side effects inside an assert.

## Crashes

The crash handler catches what the rest can't: access violations, stack overflows and `std::terminate`. It logs a symbolized stack trace and writes a minidump to:

- `<project>/.koi/Crashes/` in the editor
- `<exe>/.koi/Crashes/` in a standalone build

Open the `.dmp` in Visual Studio with the matching PDB to see the crash. `xmake install` copies the PDB next to the exe for this reason.

## Debug layers

For GPU problems, run with `--d3ddebug` (D3D12 debug layer) or `--gpuvalidation` (GPU-based validation, much slower). Their messages show up in the log. See [Command line](CommandLine.md).
