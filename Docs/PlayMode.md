# Play mode

The engine is always in one of three states: `Edit`, `Playing` or `Paused`. The editor starts in `Edit`. A standalone game starts straight in `Playing`.

| | Edit | Playing | Paused |
|---|---|---|---|
| Systems (`System`, `TaskSystem`) | don't run | run | don't run |
| `AppBase::OnTick` / `OnRender` | **run** | run | run |
| Rendering | yes | yes | yes |

`OnBeginPlay` fires when the state leaves Edit, and `OnEndPlay` when it returns. Pausing and resuming don't fire either one. Check `GetEngine()->IsPlaying()` or `GetPlayState()` when you need to know.

**`OnTick` runs in Edit too.** Put gameplay logic in systems or check `IsPlaying()`, or it will run while you're editing the scene.

## Play and Stop in the editor

**Ctrl+P** toggles Play, and **Ctrl+Shift+P** pauses. On Play, the editor saves the world to TOML exactly as it would save a scene file. On Stop, it clears the world and loads that snapshot back. What that means:

- **Only what a scene file can hold survives Stop.** Registered components are restored. `rfl::Skip<>` fields return to their defaults. Anything that isn't a registered component (unregistered data, singletons on the root) is lost.
- **Entities spawned during Play are removed** on Stop.
- **`AppBase::LoadScene` during Play** reloads the world in place: the old entities get `EndPlay`, the new ones `BeginPlay`, and singletons are cleared.
- **Your `AppBase` subclass is not reset.** Member variables keep whatever Play left in them. Reset them in `OnBeginPlay` or `OnEndPlay`.
- **Entity IDs change** on Stop, because the world is rebuilt. Don't keep `koi::Entity` handles across a Stop. The editor clears the selection for the same reason.
- **Edits you make during Play are thrown away** on Stop, including changes in the inspector and gizmo moves. The play tint on the editor is your reminder.
- Assets stay loaded across Stop, so restoring doesn't read them from disk again.

Editor code can react with `PlaySession::OnStarted` / `OnStopped` (see [Editor extensions](EditorExtensions.md)).

## When game code throws

In the editor, an exception from a system, `OnTick`, `OnBeginPlay` or `OnEndPlay` **stops Play**. The error and its stack trace go to the Log panel and a toast, and the world goes back to the snapshot. You can fix the code, rebuild and press Play again.

In Edit, nothing catches an exception from `OnTick`, so it closes the editor. The same goes for a shipped game at any time. See [Error handling](ErrorHandling.md).

## Other things to know

- Fixed-step systems (`using Phase = koi::FixedUpdate;`) run at 60 Hz, as many times per frame as they're behind. The frame delta is clamped, so a long stall can't queue up hundreds of steps.
- There's no C++ hot reload, and none is planned. Changing code means closing the editor, rebuilding and starting it again. Shaders do reload (F5).
- **Ctrl+S saves whatever the world looks like right now.** Pressing it during Play writes the play-time state into your scene file. Save in Edit.
