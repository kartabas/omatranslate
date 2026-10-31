# Omatranslate

A reading companion for PDFs on Omarchy. Read in your PDF reader as you
always do; highlight a passage, click the small Translate chip that appears
beside it, and the translation opens right there, over the page. Everything
you translate during a session also gathers in the app's window, to export as
Markdown when you're done.

![Omatranslate translating a passage](docs/screenshot.png)

The PDF is opened in your own reader and is never changed. Translations are
kept for the session only, and written beside the PDF when you export them.

## Install

```sh
git clone https://github.com/kartabas/omatranslate omatranslate
cd omatranslate
./bin/install
```

`bin/install` builds the app and installs it with `makepkg`. It then appears
in the app launcher (`Super + Space`) and in the "Open with" menu for PDFs.
It does not take over as your default PDF reader.

Requirements, all part of Omarchy: `qt6-base`, `qt6-declarative`,
`qt6-wayland`, `qt6-svg`, `layer-shell-qt`, `xdg-desktop-portal`,
`wl-clipboard`, `xdg-utils`, plus `gcc` and `make` to build.

## Reading

1. Open a PDF with `Ctrl+O`, or start the app with a file:
   `omatranslate open paper.pdf`.
2. Select a passage in your PDF reader. Omatranslate watches the Wayland
   primary selection, so a small **Translate** chip appears where the
   selection ended.
3. Click the chip. The translation opens in a card beside the passage, with
   **Copy** and **Close**; the reader keeps the keyboard, so its own shortcuts
   keep working. Pressing `Enter` in Omatranslate does the same thing.
4. The card closes on **Close**, on the next highlight, or when you quit.
   Every translation is also added to the session in the app window.

The card floats above the page rather than scrolling with it, and works while
Omatranslate is running. It needs Hyprland to know where the highlight is; if
that isn't available, the translation still appears in the app window.

Press `Space` to copy the translation, or drag it out of the window. Press
`Ctrl+E` to export the session to a sidecar Markdown file,
`paper.omatranslate.md`, beside the PDF. If your reader shows a "Save as"
dialog, that file is what you chose.

Nothing is written until you export, and quitting with unexported
translations asks first.

## Settings

The language pair sits in the top bar: click it to choose the source and
target without leaving the page. The gear (or `Ctrl+,`) opens the same
choices with the rest of the settings. The source can be auto-detected; the
target starts as your Omarchy language and can be any of the languages Google
Translate accepts.

By default a highlight waits for you to click **Translate**. Turn on
"Translate new highlights" in the gear if you would rather have the card open
as soon as you let go.

## Keyboard

| Key | What it does |
| --- | --- |
| `Ctrl+O` | Open a PDF in your reader |
| `Space` | Copy the translation |
| `Enter` | Translate the passage now |
| `Ctrl+E` | Export the session as Markdown |
| `Ctrl+Z` | Undo removing translations |
| `Ctrl+,` | Choose the languages |
| `?` | Show every shortcut |
| `Q` | Quit |
| `Esc` | Close this list |

## Command line

The same binary works without a display, for scripts and coding agents.

```sh
omatranslate translate --to de "The cat sat on the mat."
omatranslate translate --to pt --json "Good morning"
omatranslate translate --from en --to da - < passage.txt
omatranslate languages              # every code Google Translate accepts
omatranslate languages --json
omatranslate help [command]
omatranslate skill                  # the agent skill; skill install copies it
```

`--to` is required; `--from` defaults to `auto`. Each command exits 0 on
success and 1 on failure, with the error on stderr. `--json` reports the
detected source language alongside the translation.

`omatranslate skill install` writes the bundled skill to
`~/.agents/skills/omatranslate` and links it into `~/.claude/skills` when
that directory exists, so agents know how to translate with the app.

## Google Translate

Omatranslate uses Google Translate's unofficial web endpoint. The passage you
highlight is sent to Google, and only that passage; the rest of the document
stays on your machine. There is no account and no API key, so requests can be
rate-limited; the app says so and lets you try again. If that trade-off isn't
right for a document, don't highlight from it.

## Languages

The interface follows the Omarchy language (`omarchy language de`, for
example). English, German, and Danish are translated; anything else falls
back to English. Translation targets are independent of the interface
language: a German interface can translate into Danish.

## Development

```sh
./bin/build     # qmake6 + make into build/omatranslate
./bin/test      # Qt Test offscreen, plus the command-line checks
./bin/install   # build, package, and install
```

## License

MIT. Parts of the theme, translator, and portal file picker follow Omacut,
Signed, and Monologue by David Heinemeier Hansson, also MIT.
