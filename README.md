# Omatranslate

A reading companion for PDFs on Omarchy. Read in your PDF reader as you
always do; highlight a passage, click the small Translate chip that appears
beside it, and the translation opens right there, over the page. Everything
you translate during a session also gathers in the app's window, to export as
Markdown when you're done.

Omatranslate runs as an Omarchy bar widget, so watching can be switched on or
off from the bar. Nothing is ever translated just because you highlighted
something: the chip is an offer, and only pressing Translate sends a request.

![Omatranslate translating a passage](docs/screenshot.png)

The PDF is opened in your own reader and is never changed. Translations are
kept for the session only, and written beside the PDF when you export them.

## Install

```sh
git clone https://github.com/kartabas/omatranslate omatranslate
cd omatranslate
./bin/install
```

`bin/install` builds the app, installs it with `makepkg`, and puts the
Omatranslate widget in the Omarchy bar. It then appears in the app launcher
(`Super + Space`) and in the "Open with" menu for PDFs. It does not take over
as your default PDF reader.

If the app is installed already, `omatranslate plugin install` adds or
refreshes only the bar widget. `omatranslate plugin remove` takes it out
again. Both leave every other bar widget where it was.

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

## The bar widget

The bar widget is the on/off switch for the background service. It shows
whether Omatranslate is watching, paused, or off, and its menu opens the
session window or Settings.

- **Watch for highlights** on starts the hidden service. Highlights then show
  the Translate chip.
- Switching it off runs `omatranslate stop`: watching stops and the process
  quits, unless the session still has translations you haven't exported. In
  that case the window stays open so you can export first, and the app says
  so.

The same switches work from a terminal:

```sh
omatranslate enable     # start watching in the background
omatranslate disable    # pause, keeping the session open
omatranslate stop       # pause and quit, unless a session needs exporting
omatranslate quit       # quit, service included
omatranslate status     # what is running, watching, or paused
```

## Settings

The language pair sits in the top bar: click it to choose the source and
target without leaving the page. The gear (or `Ctrl+,`) opens the same
choices, the translation service, and the background switch. The source can
be auto-detected; the target starts as your Omarchy language and can be any
of the languages the chosen service accepts.

Highlighting never translates by itself. A highlight only offers the chip;
click it, or press `Enter` in the window, to send the passage.

### Translation service

Settings lets you pick where translations are requested:

- **Google Translate (default)** — no account or key, using the endpoint
  Google's own browser extension calls.
- **DeepL** — your own API key. Keys ending in `:fx` use DeepL's free host;
  other keys use the paid host.
- **LibreTranslate** — a server you name, such as your own, with an optional
  API key. A bare host gets `/translate` appended.

The settings sheet always spells out the exact address the app will use, what
is sent there, and where credentials live. Selected text and credentials go
only to that service. DeepL and LibreTranslate credentials are stored in
`~/.config/omatranslate/omatranslate.ini`, readable only by you
(`0600`). Google requests use no account or key.

Nothing is translated until you press Translate. Omatranslate keeps no copy
after you quit unless you export it.

## Keyboard

| Key | What it does |
| --- | --- |
| `Ctrl+O` | Open a PDF in your reader |
| `Space` | Copy the translation |
| `Enter` | Translate the passage now |
| `Ctrl+E` | Export the session as Markdown |
| `Ctrl+Z` | Undo removing translations |
| `Ctrl+,` | Open Settings |
| `?` | Show every shortcut |
| `Q` | Quit |
| `Esc` | Close this list |

## Command line

The same binary works without a display, for scripts and coding agents.

```sh
omatranslate translate --to de "The cat sat on the mat."
omatranslate translate --to pt --json "Good morning"
omatranslate translate --from en --to da - < passage.txt
omatranslate languages              # every code the services accept
omatranslate languages --json
omatranslate status --json
omatranslate plugin install         # add the Omarchy bar widget
omatranslate help [command]
omatranslate skill                  # the agent skill; skill install copies it
```

`translate` uses whichever service is selected in Settings, so a script and
the window never disagree about where text is sent. `--to` is required;
`--from` defaults to `auto`. Each command exits 0 on success and 1 on
failure, with the error on stderr. `--json` reports the detected source
language and the provider alongside the translation.

`omatranslate skill install` writes the bundled skill to
`~/.agents/skills/omatranslate` and links it into `~/.claude/skills` when
that directory exists, so agents know how to translate with the app.

## Languages

The interface follows the Omarchy language (`omarchy language de`, for
example). English, German, and Danish are translated; anything else falls
back to English. Translation targets are independent of the interface
language: a German interface can translate into Danish.

## Development

```sh
./bin/build     # qmake6 + make into build/omatranslate
./bin/test      # Qt Test offscreen, plus the command-line checks
./bin/install   # build, package, install, and add the bar widget
```

## License

MIT. Parts of the theme, translator, and portal file picker follow Omacut,
Signed, and Monologue by David Heinemeier Hansson, also MIT.
