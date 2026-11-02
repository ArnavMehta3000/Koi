# Input

Game code reads **actions** defined in the project TOML, not raw keys:

```toml
[input]
stick_deadzone   = 0.2    # radial, on each stick
trigger_deadzone = 0.05

[input.actions.MoveX]
negative = [ "A", "Left" ]
positive = [ "D", "Right" ]
axes     = [ "Gamepad.LeftX" ]

[input.actions.Jump]
buttons = [ "Space", "Gamepad.A" ]
```

```cpp
auto& input = GetEngine()->GetSubsystem<koi::InputManager>();

const koi::f32 move = input.GetAction("MoveX");  // -1 .. 1
if (input.IsActionPressed("Jump")) { /* ... */ }
```

## How an action works

- Every action is one `f32` in [-1, 1]. A key or button in `positive` (or `buttons`, which means the same thing) gives +1 while held. One in `negative` gives -1. Each entry in `axes` gives the stick or trigger's value after the deadzone.
- **The strongest binding wins.** Bindings aren't added together. Holding A and D at once gives whichever is listed first, not 0.
- `IsActionDown` is true while |value| ≥ 0.5. `IsActionPressed` / `IsActionReleased` are true on the frame it changes.
- Put `-` in front of an axis to flip it: `"-Gamepad.LeftY"`.
- An action name that isn't defined reads 0 and logs nothing. Check your spelling.
- A binding name that doesn't exist logs a warning when the project loads.

## Binding names

**Keys** are physical positions, read from raw scancodes. `"W"` means the key in the W spot on a QWERTY keyboard, whatever the layout.

| | |
|---|---|
| Letters | `A` ... `Z` |
| Number row | `Num0` ... `Num9` |
| Keypad | `KP0` ... `KP9`, `KPDecimal`, `KPDivide`, `KPMultiply`, `KPSubtract`, `KPAdd`, `KPEnter`, `KPEqual` |
| Function | `F1` ... `F25` |
| Arrows / navigation | `Up`, `Down`, `Left`, `Right`, `PageUp`, `PageDown`, `Home`, `End`, `Insert`, `Delete` |
| Editing | `Space`, `Enter`, `Tab`, `Backspace`, `Escape` |
| Modifiers | `LeftShift`, `RightShift`, `LeftControl`, `RightControl`, `LeftAlt`, `RightAlt`, `LeftSuper`, `RightSuper`, `Menu` |
| Punctuation | `Apostrophe`, `Comma`, `Minus`, `Period`, `Slash`, `Semicolon`, `Equal`, `LeftBracket`, `RightBracket`, `Backslash`, `GraveAccent` |
| Locks / misc | `CapsLock`, `ScrollLock`, `NumLock`, `PrintScreen`, `Pause` |

**Gamepad** names start with `Gamepad.`:

| | |
|---|---|
| Buttons | `A`, `B`, `X`, `Y`, `LeftBumper`, `RightBumper`, `Back`, `Start`, `Guide`, `LeftThumb`, `RightThumb`, `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight` |
| Axes | `LeftX`, `LeftY`, `RightX`, `RightY`, `LeftTrigger`, `RightTrigger` |

Stick up is positive Y. Mouse buttons can't be bound to actions yet. Read them from the device directly.

## Focus

**In the editor, actions read 0 unless the game viewport is hovered or you're dragging in it.** If input "doesn't work", move the mouse over the viewport. While the game has focus, the editor turns off ImGui keyboard navigation so your keys don't also move through the UI. Gamepad rumble only plays while the game is focused and in Play.

The window losing focus releases every key, so nothing gets stuck down.

## Devices directly

For anything actions don't cover, read the devices. These reads **ignore focus**, which is why the editor camera uses them:

```cpp
input.GetKeyboard().IsDown(koi::KeyCode::LeftShift);
input.GetMouse().GetDelta();                          // also GetPosition, GetScroll, IsButtonDown/Pressed/Released
input.GetMouse().SetCursorLocked(true);               // hidden and held in the window centre; deltas keep coming
input.GetGamepad().GetAxis(koi::GamepadAxis::RightTrigger);
input.GetGamepad().SetRumble({ .Low = 0.5f, .High = 0.2f });
```

Only the first connected gamepad is used. Keyboard and mouse use Win32 raw input, and the gamepad uses GameInput.
