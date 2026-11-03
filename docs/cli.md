# Command line

The `omatranslate` binary has two halves: the window and the commands. The
commands need no display, so they work over SSH, in scripts, and for coding
agents. `omatranslate help <command>` prints the same options.

```sh
omatranslate <command> [options]
omatranslate --version
omatranslate help
```

## Opening the app

| Command | What it does |
| --- | --- |
| `omatranslate open [pdf]` | Starts the background service if needed, opens the PDF in your reader, and shows the window |
| `omatranslate settings` | Shows the window with Settings open |
| `omatranslate show` | Shows the window of a running app (starting it if needed) |
| `omatranslate hide` | Hides the window, keeping the service and session alive |
| `omatranslate service [--enable\|--paused]` | Runs the background service directly; the bar widget uses this |

These are views onto one process. `open`, `settings`, `show`, and `hide`
forward to a running service over its control socket; if none is running they
start one, hidden, and then forward. There is never a second copy watching the
selection.

`service` is what the widget and the `enable`/`open` commands start. `--enable`
starts with watching on, `--paused` with watching off; without a flag it uses
the saved `enabled` setting.

## Translating

```sh
omatranslate translate [--from <code>] --to <code> [--json] <text|-> 
```

| Option | Meaning |
| --- | --- |
| `--to <code>` | Target language, required (`de`, `pt-BR`, …) |
| `--from <code>` | Source language, default `auto` |
| `--json` | Print a JSON object instead of plain text |
| `-` | Read the passage from standard input |

The command uses the service chosen in Settings and the same chunking as the
window, so a long passage is fine. Examples:

```sh
omatranslate translate --to de "The cat sat on the mat."
omatranslate translate --to pt --json "Good morning"
omatranslate translate --from en --to da - < passage.txt
```

Plain output is just the translation. JSON output:

```json
{
  "detected": "en",
  "provider": "google",
  "target": "de",
  "translation": "Guten Morgen"
}
```

`detected` is the language the service recognised, `provider` is the id
(`google`, `deepl`, or `libretranslate`), and `target` echoes the requested
code.

Errors are always plain text on stderr, even with `--json`, and the exit code
is 1:

```sh
$ omatranslate translate --to xx "hi"
Unknown target language xx. omatranslate languages lists them.
```

## Languages

```sh
omatranslate languages          # code, tab, name — one per line
omatranslate languages --json
```

```json
{
  "languages": [
    { "code": "auto", "english": "", "name": "Auto-detect" },
    { "code": "de", "english": "German", "name": "Deutsch" },
    { "code": "en", "english": "English", "name": "English" }
  ]
}
```

The list is the union of what the three services accept. A code may still be
rejected by the service you have selected; the error says so.

## Watching and the service

| Command | Service | Watching | Session |
| --- | --- | --- | --- |
| `omatranslate enable` | starts if needed | on | kept |
| `omatranslate disable` | keeps running | off | kept |
| `omatranslate stop` | quits | off | kept only if unexported, with the window open |
| `omatranslate quit` | quits | — | discarded |

`enable` and `disable` persist the choice, so the next service start remembers
it. `stop` refuses to end a session that has translations you have not
exported; it keeps the window open and reports `keptOpen: true`.

All four accept `--json`:

```json
{"ok":true,"enabled":true,"started":true,"running":true}
{"ok":true,"enabled":false,"running":true,"keptOpen":true}
{"ok":true,"enabled":false,"running":false,"stopped":true,"keptOpen":false}
{"ok":true,"wasRunning":true}
```

Those are `enable`, `disable`, `stop`, and `quit`, in that order.

## Status

```sh
omatranslate status
omatranslate status --json
```

Without `--json` it prints one human sentence. With it:

```json
{
  "ok": true,
  "running": true,
  "service": true,
  "enabled": true,
  "pid": 1958008,
  "version": "0.2.0",
  "status": "idle",
  "historyCount": 10,
  "document": "",
  "targetLanguage": "uk",
  "provider": "google",
  "providerName": "Google Translate",
  "endpoint": "https://clients5.google.com/translate_a/t"
}
```

| Field | Meaning |
| --- | --- |
| `running` | a process answered |
| `service` | it is the hidden background service, not a lone window |
| `enabled` | watching is on |
| `status` | `idle`, `translating`, `done`, or `error` |
| `historyCount` | passages in this session |
| `document` | the open PDF, if any |
| `provider`, `providerName`, `endpoint` | where translations go |

`status` works without a running app: it then reads the saved settings and
reports `running: false`. This is what the bar widget polls.

## The bar widget

```sh
omatranslate plugin install    # add or refresh it
omatranslate plugin remove     # take it out
omatranslate plugin            # report whether it is installed
omatranslate plugin --json
```

```json
{"ok":true,"installed":"/home/you/.config/omarchy/plugins/kartabas.omatranslate","bar":"/home/you/.config/omarchy/shell.json"}
```

See [The bar widget](bar-widget.md).

## Agent skill

```sh
omatranslate skill             # print the bundled skill
omatranslate skill install     # copy it to ~/.agents/skills/omatranslate
```

When `~/.claude` exists, `skill install` also links the skill into
`~/.claude/skills/omatranslate`. With `--json` it prints `installed` and,
when linked, `linked`.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | success, including “wasn't running” for `quit` |
| 1 | the request failed; the reason is on stderr |

## Scripts

```sh
# Translate every paragraph of a file into German, one request per line
while IFS= read -r line; do
  omatranslate translate --from en --to de "$line"
done < paragraphs.txt

# Watch state for a status bar or a hook
omatranslate status --json | jq -r '.enabled'

# Start the background service at login
omatranslate enable
```
