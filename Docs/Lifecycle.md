# Lifecycle

How an exe gets from `main` to the main loop, what one frame does, and how it shuts down.
Four layers: **Launcher** (`Launcher/*Main.cpp`), **Engine** (`Koi/Engine`), **Editor** (`Koi/Editor`, `editor-*` modes only)
and **Game** (the project's `AppBase` subclass + its `World`). In `game-*` modes the Editor column doesn't exist and the
Engine calls the game's `AppBase` directly; steps `EditorApp` does itself don't happen.

## 1. Initialization

```mermaid
sequenceDiagram
	autonumber
	box Launcher
		participant Main as EditorMain / RuntimeMain
	end
	box Engine
		participant Run as RunApplication
		participant Eng as Engine
		participant Sub as InputManager / Renderer / AssetRegistry
	end
	box Editor
		participant Ed as EditorApp
	end
	box Game
		participant App as AppBase (game)
		participant W as World
	end

	Main->>Run: LaunchDesc (editor: baked dirs + MakeEditorHost, CreateApp = AppBase::Create)
	Run->>Run: LoggerScope (console only), CrashHandler::Install
	Run->>Run: ParseArgs, pick .koi/ dir, find project TOML (editor: baked, runtime: FindCookedRoot)
	Run->>Run: LoadProjectDesc → AppCreateDesc, Logger::Configure (+ --log overrides)
	Run->>Run: VFS mount engine:// project:// (editor: source + build dirs, runtime: cooked)

	Run->>Eng: Engine(Desc)
	Eng->>Sub: construct InputManager, Renderer, AssetRegistry
	Note over Sub: Renderer ctor creates GfxDevice, graphics queue,<br/>PipelineCache, default resources, material templates
	Eng->>Eng: ComponentRegistry::RegisterEngineComponents

	Run->>App: desc.CreateApp(cmd) → game's AppBase::Create
	Run->>App: SetDesc(appDesc)
	opt editor build (desc.Adapter set)
		Run->>Ed: MakeEditorHost(app) → EditorApp owns the game, forces windowed
	end

	Run->>Eng: RunApp(host)
	Eng->>Eng: create Window from host.GetDesc().WindowDesc
	Eng->>Ed: SetWindow
	Ed->>App: SetWindow
	Eng->>Sub: Input.Init (raw input, actions)
	Eng->>Sub: Renderer.Init (swapchain, frame cmd list, materials, "Deferred" main SceneRenderer)
	Eng->>Sub: Assets.Init (AssetIndex::Rebuild)
	Eng->>Eng: window close callback → RequestExit

	Eng->>Ed: HostInit
	Ed->>Ed: Setup: overlay hook, EditorContext services<br/>(Selection, AssetDatabase.Refresh, ImportQueue, Inspectors,<br/>PlaySession, Undo, SceneFiles, Commands), panels, file drop callback
	Ed->>Ed: read .koi/Editor.toml, ReopenLastScene (swaps StartupScene, or queues an unlisted scene for the first tick)
	Ed->>App: HostInit
	App->>App: OnRegisterComponents / Systems / Modules
	App->>W: World() builds ECS, RenderScene, systems + observers
	App->>W: LoadStartupScene → World::Load (propagates transforms, syncs RenderScene) → OnSceneLoaded
	App->>App: OnInit
	Ed->>Ed: StartExtensions (KOI_EDITOR_EXTENSION → OnStartup)
	Ed->>Ed: ImGui init, panel settings, LoadEditorState

	opt host.StartsInPlayMode() (standalone game)
		Eng->>Eng: SetPlayState(Playing)
		Eng->>App: HostBeginPlay → World::BeginPlay, OnBeginPlay
	end
	Eng->>Eng: MainLoop
```

## 2. Main loop

One iteration of `Engine::MainLoop`, repeated while `m_isRunning`.

```mermaid
sequenceDiagram
	autonumber
	box Engine
		participant Eng as Engine
		participant In as InputManager / Window
		participant R as Renderer
		participant SR as SceneRenderer(s)
		participant AR as AssetRegistry
	end
	box Editor
		participant Ed as EditorApp
		participant P as Panels / Context
	end
	box Game
		participant App as AppBase (game)
		participant W as World / ECS
	end

	rect rgba(128,128,128,0.08)
	Note over Eng,AR: BeginFrame
	Eng->>In: Input.BeginFrame(IsPlaying), Window::PollEvents
	Eng->>R: BeginFrame: apply pending resize, GPU profiler frame, open cmd list
	end

	rect rgba(128,128,128,0.08)
	Note over Eng,W: Tick
	Eng->>Eng: Timer.Tick → dt
	Eng->>Ed: HostTick(dt)
	Ed->>R: pending shader reload (F5) before anything records
	Ed->>P: Context.Tick, Panels.TickAll (viewport sets game focus), ImportQueue.Update
	Ed->>App: HostTick(dt) (throw in play → report + Stop)
	App->>W: World::Update(dt)
	alt Edit / Paused
		W->>W: ECS.Sync, PropagateTransforms, RenderScene.Sync
	else Playing
		loop while accumulator ≥ fixed step
			W->>W: fixed phases (Sync at each phase end), PropagateTransforms
		end
		W->>W: frame phases, PropagateTransforms, RenderScene.Sync
		Note over W: a failed system skips the rest of Update, which then throws
	end
	App->>App: OnTick(dt)
	App->>W: pending LoadScene → World::Load in place (propagates + syncs) → OnSceneLoaded
	end

	rect rgba(128,128,128,0.08)
	Note over Eng,W: Render (records, doesn't execute)
	Eng->>Ed: HostRender
	Ed->>P: SceneViewport.PrepareFrame
	Ed->>App: HostRenderInto(viewport view)
	App->>SR: main SceneRenderer.Submit(view, RenderScene): features record graph, Enqueue
	App->>App: OnRender
	Ed->>Ed: QueueGizmos (DebugDraw)
	Ed->>P: Panels.RenderAll (asset viewer submits its own SceneRenderer)
	end

	rect rgba(128,128,128,0.08)
	Note over Eng,AR: EndFrame
	Eng->>R: EndFrame
	opt nothing submitted and no overlay
		R->>R: clear back buffer to black
	end
	R->>SR: Execute each submitted graph, in submit order
	R->>R: DebugDraw.Clear
	R->>Ed: OnOverlayRender: ImGui frame, menu, panels, shortcuts, palette,<br/>FlushShaderReadRequests, clear back buffer, ImGui draw
	R->>R: resolve GPU timers, back buffer → PRESENT, Submit, Present
	Eng->>AR: Collect (free assets unused > kUnusedGraceSeconds)
	Eng->>Eng: FrameMark, --timeout check
	end
```

Standalone, `HostTick` / `HostRender` go straight to `AppBase`, which renders `GetWindowView()` (the back buffer) and has no overlay.

Play transitions run immediately inside `Engine::SetPlayState`. The editor's play controls call it through `PlaySession`
during `OnOverlayRender`, so `HostBeginPlay` / `HostEndPlay` run inside `Renderer::EndFrame`:

```mermaid
flowchart LR
	Edit -- "Play: PlaySession snapshots world" --> Playing
	Playing -- Pause --> Paused
	Paused -- Resume --> Playing
	Playing -- "Stop: restore snapshot" --> Edit
	Paused -- Stop --> Edit
	Edit -. "entering play: HostBeginPlay → World::BeginPlay, OnBeginPlay" .-> Playing
	Playing -. "leaving play: HostEndPlay → OnEndPlay, World::EndPlay" .-> Edit
	Paused -. "leaving play: HostEndPlay" .-> Edit
```

## 3. Shutdown

Starts when `RequestExit` clears `m_isRunning` (window close, File → Exit, `--timeout`, or game code). Teardown is split between
`RunApp`'s tail and scope exit in `RunApplication`; the order matters because the host frees GPU resources in its
destructor, so the device lives until `~Engine`.

```mermaid
sequenceDiagram
	autonumber
	box Engine
		participant Run as RunApplication
		participant Eng as Engine
		participant Sub as InputManager / Renderer / AssetRegistry
	end
	box Editor
		participant Ed as EditorApp
	end
	box Game
		participant App as AppBase (game)
		participant W as World
	end

	Eng->>Eng: MainLoop exits
	Eng->>Eng: SetPlayState(Edit)
	opt was in play
		Eng->>Ed: HostEndPlay
		Ed->>App: HostEndPlay → OnEndPlay, World::EndPlay
	end

	Eng->>Ed: HostShutdown
	Ed->>Ed: clear drop callback, SaveEditorState (.koi/Editor.toml)
	Ed->>Ed: ShutdownExtensions (reverse order)
	Ed->>Ed: EditorContext.Shutdown (cancel + wait for imports)
	Ed->>App: HostShutdown
	App->>App: OnShutdown
	App->>W: World::Clear (component hooks release asset refs)
	Ed->>Ed: ImGui shutdown
	Eng-->>Run: RunApp returns

	Run->>Ed: ~EditorApp (host unique_ptr leaves scope)
	Ed->>Ed: ~EditorContext: services + panels, newest first
	Ed->>App: ~AppBase (owned m_game)
	App->>W: ~World → ~ECS, ~RenderScene
	Note over Run,App: standalone: ~AppBase runs here instead (app declared after engine)

	Run->>Eng: ~Engine
	Eng->>Sub: AssetRegistry.Shutdown (ReleaseAll every cache), destroy
	Eng->>Sub: ~Renderer: WaitForIdle, SceneRenderer, capture, frame ctx,<br/>swapchain, materials, PipelineCache, default resources, device
	Eng->>Sub: ~InputManager
	Eng->>Eng: destroy Window, s_instance = null
	Run->>Run: ~LoggerScope → Logger::Shutdown (also outlives catch handlers)
```

Any exception escaping the loop unwinds the same scopes (skipping `HostShutdown`) and is logged with its trace by
`RunApplication`'s catch blocks, which return 1 (std::exception) or 2 (unknown).
