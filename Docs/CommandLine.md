# Command line

No Koi exe needs arguments. The editor has its engine and project paths baked in at build time. A standalone build looks for its cooked content in `<exe>/`, `<exe>/Cooked`, the working directory, or `<cwd>/Cooked`, and uses the first one that has a `Project/Project.koi.toml`.

## Built-in flags

| Flag | |
|---|---|
| `-p, --project <file>` | Use a different project TOML |
| `--d3ddebug` | Turn on the D3D12 debug layer |
| `--gpuvalidation` | Turn on GPU-based validation (implies `--d3ddebug`) |
| `--log <Cat=Level,...>` | Override log levels for this run |

`--log` beats both `[logging] level` and `[logging.categories]` in the project file. `all=<level>` resets every category first, and then any named categories apply on top:

```sh
Sandbox-Editor.exe --log all=Warn,Render=Trace
```

Levels are `Trace`, `Debug`, `Info`, `Warn`, `Error` and `Critical`. Levels below the `--LogLevel` you built with aren't compiled in, so `--log` can't bring them back. See [Logging](Logging.md).

## Adding your own flags

The parser is [CLI11](https://github.com/CLIUtils/CLI11). To add flags, subclass `koi::CommandLine`, declare your options in the constructor, and register the class once with a static `CommandLineRegistrar`:

```cpp
// Source/MyGameCommandLine.h
#include "Koi/Engine/Runtime/CommandLine.h"

class MyGameCommandLine : public koi::CommandLine
{
public:
	MyGameCommandLine()
	{
		add_flag("--god-mode", m_godMode, "Start invincible");
		add_option("--level", m_level, "Scene to load instead of [scenes] startup");
	}

	bool IsGodMode() const noexcept { return m_godMode; }
	const std::string& GetLevel() const noexcept { return m_level; }

private:
	bool        m_godMode = false;
	std::string m_level;
};
```

```cpp
// Source/MyGameApp.cpp
static koi::CommandLineRegistrar<MyGameCommandLine> s_cmdReg;

std::unique_ptr<koi::AppBase> koi::AppBase::Create(koi::CommandLine& cmd)
{
	auto& args = static_cast<MyGameCommandLine&>(cmd);
	return std::make_unique<MyGameApp>(args.IsGodMode());
}
```

The engine parses the command line before it calls `AppBase::Create`, so the values are already set when you read them. The built-in flags keep working. If parsing fails, the error is logged and the exe exits. [Customization](Customization.md) lists the other things a game can change.
