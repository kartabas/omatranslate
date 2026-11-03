# Troubleshooting

Start here:

```sh
omatranslate status --json
journalctl --user -f | grep -i omatranslate
```

`status` says whether a process is running, whether it is watching, and which
provider and endpoint it will use. The journal shows QML warnings and provider
errors.

## The Translate chip does not appear

| Check | Fix |
| --- | --- |
| `omatranslate status --json` says `running: false` | Start it: **Watching** in the bar, or `omatranslate enable` |
| `enabled: false` | Choose **Watching**, or `omatranslate enable` |
| `wl-clipboard` missing | `omarchy-pkg-add wl-clipboard`, then restart the service |
| The reader is an X11 app | The primary selection is a Wayland feature; use a Wayland reader |
| The reader copies to the clipboard, not the primary selection | Select the text again; some readers need a middle-click copy or an explicit “copy” |
| Another clipboard tool clears the selection | Stop it, or set the reader to keep the selection |

The watcher polls every 500 ms. A very long selection (over 200 000
characters) is ignored on purpose.

## The chip appears, but no card follows

The card is placed with Hyprland. Check that the service inherited the
Hyprland session:

```sh
tr '\0' '\n' < /proc/$(omatranslate status --json | jq .pid)/environ | grep HYPRLAND
```

If `HYPRLAND_INSTANCE_SIGNATURE` is missing, the process was started from an
environment without the compositor — common when a command was launched from
SSH or a systemd unit. Quit and start it from the bar or a terminal inside the
session. Without Hyprland the translation still appears in the main window;
only the floating card is unavailable.

## The window opens offscreen, or not at all

`QT_QPA_PLATFORM` or `QT_QPA_PLATFORMTHEME` is set in the shell that launched
it (some test harnesses and SSH sessions set `offscreen`). Omatranslate strips
those variables when it starts the service itself, but not when it is the
service. Check:

```sh
env | grep QT_QPA
```

Unset them and run `omatranslate open` again.

## A second copy is running, or the widget does nothing

Only one process can own the socket. `omatranslate status --json` reports its
`pid`. A second `service` exits 0 by design. If a process is stuck, end it
with

```sh
omatranslate quit
```

or kill the pid from `status`, not with `pkill -f "omatranslate service"` —
that pattern also matches the shell running the command.

## The bar widget is missing

```sh
omatranslate plugin --json
```

- `installed: false` → `omatranslate plugin install`.
- Installed, but no widget → check that
  `~/.config/omarchy/shell.json` has
  `{"id": "kartabas.omatranslate"}` in a `bar.layout` section, then run
  `omarchy-restart-shell`.
- The widget is there but shows an older panel after an update → Quickshell
  keeps the compiled component for an unchanged path. Run
  `omarchy-restart-shell` once.

## The widget says Stopped after login

The widget's `enabled` flag starts the service when the bar comes up, but the
app's own saved setting may be `enabled=false` after a **Stop**. Choose
**Watching** once; the choice is remembered.

## Stop leaves the window open

That is deliberate: the session has translations you have not exported.
Export with `Ctrl+E`, then stop again, or `omatranslate quit` to discard them.

## Google Translate fails or returns nothing

The `clients5.google.com` endpoint is unofficial. It can answer with an empty
result under load; press **Retry** in the window or the card. If it keeps
failing, switch to DeepL or a LibreTranslate server in Settings. A network
error names the service in the window, so a failure is never silent.

## DeepL says the key is missing or rejected

- Paste the key in **Settings → Translation service → DeepL**.
- Free keys end in `:fx` and use `api-free.deepl.com`; a paid key must not end
  in `:fx`.
- A `403` usually means the key is wrong or the free quota is exhausted.

## LibreTranslate returns 403 or 429

- A bare host gets `/translate` appended; a full path is used as typed.
- Many public servers require an API key. Add it in Settings.
- `429` is rate limiting; use your own server or wait.

## Export does nothing

Export goes through `xdg-desktop-portal`. Check that it is installed and
running:

```sh
systemctl --user status xdg-desktop-portal
```

If the dialog cannot appear, the export is cancelled and the session stays
unsaved. The folder you last chose is remembered in
`~/.config/omatranslate/omatranslate.ini`.

## The interface is in the wrong language

It follows Omarchy:

```sh
omarchy language de
```

The window retranslates immediately. English, German, and Danish are included;
other languages fall back to English. The translation target is independent —
a German interface can translate into Danish.

## Translations disappeared

The session lives in memory and is written only on export. `quit`, a crash, or
logging out discards it. `stop` refuses to do that silently: it keeps the
window open while unexported translations exist.

## Reporting a bug

Include:

- `omatranslate status --json`
- `omatranslate --version`
- your Omarchy and Qt versions (`omarchy --version`, `qmake6 -query
  QT_VERSION`)
- the relevant `journalctl --user` lines
- what you did, what you expected, and what happened

Open an issue at <https://github.com/kartabas/omatranslate/issues>.
