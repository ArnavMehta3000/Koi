# What a game can customize

Everything below is done from your project. You never need to edit the engine.

| What | How | Docs |
|---|---|---|
| Window, logging, scenes, input actions | The project TOML | [Starting a project](NewProject.md) |
| Game lifecycle | Override `AppBase` hooks: `OnInit`, `OnTick`, `OnRender`, `OnBeginPlay`, `OnEndPlay`, `OnShutdown` | [Starting a project](NewProject.md) |
| Components | A struct with `kKey`, plus `ComponentType<T>` | [Components](Components.md) |
| Scene file format | `KOI_REFLECT(...)`, or a hand-written `Serializer<T>` | [Components](Components.md) |
| Systems | `System<...>` or `TaskSystem`, in a built-in or custom phase (`using Phase = ...`) | [Components](Components.md) |
| Event reactions | `Observer<Event, Components...>` | [Components](Components.md) |
| Command-line flags | Subclass `CommandLine` and register it with `CommandLineRegistrar` | [Command line](CommandLine.md) |
| Inspectors, panels, editor commands | An `EditorExtension` in `Editor/` | [Editor extensions](EditorExtensions.md) |
| Shaders | `Content/Shaders/Shaders.json` | [Shaders](Shaders.md) |
| Input actions | `[input.actions]` in the project TOML | [Input](Input.md) |
| Begin/end play behavior | `OnBeginPlay`, `OnEndPlay` | [Play mode](PlayMode.md) |
| Render pipeline and passes | `Renderer::SetPipeline` with your own build function | [Rendering](Rendering.md) |
| Log categories and levels | `KOI_LOG_CATEGORY(Name)`, `[logging]`, `--log` | [Logging](Logging.md) |
