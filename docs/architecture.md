# Architecture

Omatranslate is one Qt process. The window, the background watcher, the
floating translation card, and the command line all belong to it; the bar
widget and the CLI are thin clients that talk to it over a local socket.

```
                    ┌─────────────────────────────────────────────┐
  Omarchy bar  ───▶ │  omatranslate (one process)                 │
  widget / CLI      │                                             │
                    │  ControlServer ── AppController ── Backend  │
                    │       │                │             │      │
                    │       │                │             ├── SelectionWatcher (wl-paste)
                    │       │                │             ├── TranslationService ──▶ provider
                    │       │                │             └── history / export
                    │       │                │                    │
                    │       │                └── Main.qml (window) │
                    │       └─────────────────── TranslationOverlay │
                    └─────────────────────────────────────────────┘
```

## One process

Starting Omatranslate always resolves to the same process:

- `omatranslate open`, `settings`, `show`, `hide`, `translate`, and the
  launcher send a command to a running service. If none answers, they start
  `omatranslate service` (hidden) and then forward the command.
- The bar widget does the same through `omatranslate enable`, `disable`,
  `stop`, `open`, and `settings`.
- A bare `omatranslate service` is what those commands start. `--enable` and
  `--paused` choose the initial watching state.

`service` mode creates the main window but hides it immediately, so the same
QML, backend, and overlay are ready when the window is asked for. Closing the
window calls `AppController::hideMain()` instead of ending the process;
`stop`/`quit` end it.

## The control socket

`ControlServer` is a `QLocalServer` named `omatranslate-<USER>`, which on Linux
lands in `/tmp/omatranslate-<USER>`. One JSON object goes in each direction:

```
→ {"command": "status"}
← {"ok": true, "running": true, ...}
```

`ControlServer::isRunning()` is a real round trip: it connects and waits for a
reply, so a stale socket file never counts as a running app. `listen()` first
checks whether another process answers and, after binding, verifies that the
socket reports its own pid; a second `service` therefore exits 0 instead of
stealing the socket or watching the selection twice.

Commands understood by `AppController::handle()`:

| Command | Effect |
| --- | --- |
| `status` | Process, session, and provider facts (see [Command line](cli.md#status)) |
| `show` / `hide` | Show or hide the main window |
| `settings` | Show the window with Settings open |
| `open` | Optionally open a PDF, then show the window |
| `translate` | Translate the current selection |
| `enable` / `disable` | Turn watching on or off |
| `stop` | Turn watching off and quit, unless unexported history exists |
| `quit` | Quit unconditionally |

Commands that need no display run with `QT_QPA_PLATFORM=offscreen` and
`QT_QPA_PLATFORMTHEME=generic`, so they work over SSH. A service started by
such a command must **not** inherit those variables: `ControlServer::startService()`
strips them, and the service comes up in the real Wayland session.

## Selection watching

`SelectionWatcher` polls the Wayland primary selection every 500 ms with

```sh
wl-paste --primary --no-newline --type text
```

one process at a time. It emits `textChanged` only for real changes and
ignores selections longer than 200 000 characters. `wl-clipboard` is the only
external requirement; without it the watcher reports a readable error and
stays idle.

Watching is gated by `Backend::enabled`, which is persisted in the ini file.
`disable` stops the timer without ending the process; `enable` starts it.

## The translation path

`Backend::translateNow()` takes the current selection and calls
`TranslationService`, which owns the provider objects:

| Provider id | Class | Endpoint |
| --- | --- | --- |
| `google` | `GoogleTranslate` | `clients5.google.com/translate_a/t` |
| `deepl` | `DeepLTranslate` | `api.deepl.com` or `api-free.deepl.com` |
| `libretranslate` | `LibreTranslate` | user-supplied, `/translate` by default |

Each provider implements `TranslationProvider` (`translate`, `cancel`,
`finished`, `failed`) and chunks long passages before sending. The service
selects the provider from the `provider` setting and exposes `providerName`
and `endpoint` for the UI and `status --json`.

A finished translation goes to `Backend`, which adds it to the history and
emits `translationChanged`; the window and the overlay both listen to that.

## History and export

History is a `QVariantList` in memory, newest first:

```json
{
  "text": "Aufmerksamkeit ist keine Lampe …",
  "translation": "Attention is not a lamp …",
  "from": "de",
  "to": "en",
  "document": "paper.pdf",
  "time": "2026-11-02T18:32:00"
}
```

- Re-translating the same `text` + `to` moves the existing entry to the top.
- The list is capped at 300 entries.
- Removing entries snapshots the list for `Ctrl+Z`.
- Export writes `# Translations`, a document and timestamp header, then one
  section per entry, oldest first, with the original as a block quote.

The sidecar name is `<pdf-base-name>.omatranslate.md`, or `omatranslate.md`
when no document is open. The save dialog comes from
`xdg-desktop-portal`; the PDF is only ever read.

## The floating card

`Overlay` is a `LayerShellQt` window with two modes: a small **Translate** chip
and the translation card. `visible` starts false and `LayerShellQt::Window::get()`
is called before the first show, which is required for the layer surface to be
configured.

Placement comes from `hyprctl monitors -j`, `cursorpos`, and
`activewindow -j`:

1. Prefer the pointer position; fall back to the centre of the active window.
2. If neither is on a known screen, use the primary screen.
3. Put the card 16 px to the right of the anchor, or to its left when it would
   overflow; keep it inside the screen with an 8 px margin.

Without Hyprland, or when no screen matches, the card is centred on the
primary screen; if the compositor offers no layer shell at all, the
translation still appears in the main window.

## Theme and language

- `Theme` reads `~/.local/state/omarchy/current/theme/colors.toml` and watches
  the directory, so colours change with the desktop theme without a restart.
- `Translator` reads `~/.config/omarchy/language`, loads the matching JSON
  catalog compiled into the binary (`src/i18n/`), and falls back to English.
  It also watches the language file, so `omarchy language de` retranslates the
  open window immediately.
- The bar widget follows the same language list (`src/omarchy/i18n/`) and the
  Omarchy bar's own theme tokens.

## Source layout

| Path | What lives there |
| --- | --- |
| `src/main.cpp` | Mode selection, process wiring, QML setup |
| `src/appcontroller.*` | Window visibility, settings request, IPC semantics |
| `src/controlserver.*` | Local socket, single-instance check, service start |
| `src/backend.*` | Selection state, history, export, settings |
| `src/selectionwatcher.*` | Polling `wl-paste` |
| `src/translationservice.*` | Provider choice, endpoint, disclosure text |
| `src/{google,deepl,libretranslate}translate.*` | The three providers |
| `src/overlay.*`, `src/placement.*` | Floating card and its geometry |
| `src/theme.*`, `src/translator.*` | Omarchy theme and interface language |
| `src/Main.qml`, `src/*.qml` | Window, settings, history rows, card |
| `src/omarchy/` | The bar widget, manifest, and its translations |
| `src/cli.cpp` | Command line and `plugin`/`skill` commands |
| `tests/` | Qt Test suite and CLI checks |
| `pkgbuild/` | Arch package metadata |

## Failure behaviour

- A second `service` exits 0; it does not steal the socket.
- A command with no running service starts one, so there is no “app is not
  running” dead end.
- `stop` with unexported history keeps the process and window alive and
  reports `keptOpen`.
- Quitting hides the window before `QCoreApplication::exit(0)`, because
  `quit()` is swallowed once a Quick window has been shown.
- A translation failure is shown in the window and the card, never as a
  silent no-op.
