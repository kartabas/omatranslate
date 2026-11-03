# Development

## Requirements

Everything is in Omarchy except the build tools:

```sh
sudo pacman -S --needed base-devel qt6-base qt6-declarative qt6-wayland \
  qt6-svg layer-shell-qt xdg-desktop-portal wl-clipboard xdg-utils
```

Qt 6 with `qmake6` is what the build uses; there is no CMake or Meson step.

## Build, test, install

```sh
./bin/build     # qmake6 + make into build/omatranslate
./bin/test      # Qt Test offscreen, then the CLI checks
./bin/install   # build, makepkg -fsi, then install the bar widget
```

`bin/install` needs `sudo` for `makepkg -i`. It also runs
`build/omatranslate plugin install`, because the widget lives in the user's
config rather than in the package.

The build is incremental; `make clean` in `build/` (or deleting it) is the
reset. `./bin/test` builds the tests separately in `build/tests/`.

## Project layout

The short version; [Architecture](architecture.md) explains how the pieces
work.

| Path | What lives there |
| --- | --- |
| `src/main.cpp` | CLI detection, mode selection, object wiring |
| `src/appcontroller.*` | Window visibility and IPC semantics |
| `src/controlserver.*` | Local socket and single-instance logic |
| `src/backend.*` | State, history, export, settings |
| `src/selectionwatcher.*` | Polling the Wayland primary selection |
| `src/translationservice.*`, `*translate.*` | Provider selection and the three providers |
| `src/overlay.*`, `src/placement.*` | Floating card and geometry |
| `src/theme.*`, `src/translator.*` | Omarchy theme and language |
| `src/*.qml` | Window, settings, rows, card |
| `src/omarchy/` | Bar widget (`Panel.qml`, manifest, i18n) |
| `src/cli.cpp` | Commands, `plugin`, `skill` |
| `tests/` | Qt Test suite and `cli_test.py` |
| `pkgbuild/` | Arch package |

## Tests

`./bin/test` runs two layers:

1. **Qt Test**, offscreen, in `tests/omatranslate_tests.cpp`. It covers
   backend behaviour (history, dedupe, undo, export, enable/disable), provider
   parsing, placement maths, and the status/IPC contract. It needs no display
   and no network.
2. **CLI checks**, `tests/cli_test.py`, against the built binary. They cover
   help, argument errors, JSON shapes, `status` with and without a service,
   and the plugin install/remove round trip against a temporary
   `XDG_CONFIG_HOME`.

Run one test while iterating:

```sh
cd build/tests && ./omatranslate_tests theStatusCommandDescribesTheSession
```

When fixing a bug, add the failing case to the suite first; the tests are
plain `QTest` functions and do not use fixtures or mocks beyond a temporary
settings file.

## Adding a translation provider

1. Implement `TranslationProvider` in `src/<name>translate.{h,cpp}`:
   `name()`, `endpoint()`, `translate()`, `cancel()`, and the `finished` /
   `failed` signals. Chunk long input with the shared helper.
2. Add the id to `TranslationService::knownProvider()` and construct it in
   `TranslationService::rebuild()`, reading any key or URL from settings.
3. Add the id to `providerName()` and `endpointFor()` so `status --json` and
   the disclosure line stay correct.
4. Add the choice and its fields to `src/SettingsPopup.qml`, with the same
   “where does this go” disclosure the other providers have.
5. Add the provider to `src/languages.cpp` if it changes the accepted
   language set, and to the provider table in
   [Translation services](translation-services.md).
6. Cover the request/response parsing in the Qt tests.

Keys belong in `saveSetting()`, which persists and re-applies `0600` to the
ini file.

## Adding an interface language

The catalogs are plain JSON keyed by the English source string, with an
optional object for plurals:

```json
{
  "Translating…": "Übersetzt …",
  "This session": {
    "one": "%1 passage in this session",
    "other": "%1 passages in this session"
  }
}
```

1. Copy `src/i18n/en.json` to `src/i18n/<code>.json` and translate the values.
2. Add the file to `src/resources.qrc`.
3. `Translator` loads `<base>` and then `<base>-<REGION>` when present, so
   `pt-BR` can build on `pt`.
4. The bar widget has its own small catalog in `src/omarchy/i18n/`; add the
   same code there and to `manifest.json` if it lists locales.

Keep keys as the English text and never translate the keys. `Text.PlainText`
is set on every label that shows a selection, translation, provider, or error,
so a passage containing `<` or `&` cannot become markup.

## Style

- C++17, four spaces, Qt naming (`camelCase` methods, `m_` members).
- QML two spaces, ids in lower camel case; follow the Omarchy UI components
  (`qs.Ui`) rather than reimplementing buttons and panels.
- Comments explain why something is done, especially around process and
  window lifetime; the code says what.
- Every displayed string goes through `tr()`/`qsTr()`; user text uses
  `Text.PlainText`.
- Keep the window usable without a mouse: add shortcuts and a cursor path for
  new controls.

## Releasing

1. Bump the version in `src/main.cpp` (`setApplicationVersion`) and
   `pkgbuild/PKGBUILD` (`pkgver`), and add the entry to `CHANGELOG.md`.
2. `./bin/build && ./bin/test`.
3. `./bin/install` on a clean session; check the launcher, **Open with**, the
   bar widget, and `omatranslate status --json`.
4. Commit, tag (`v0.2.0`), push the tag, and attach the package or a source
   archive to the GitHub release.

The bar widget is shipped inside the binary's resources, so a package upgrade
alone does not refresh the copy in `~/.config/omarchy/plugins`. Run
`omatranslate plugin install` after upgrading, and `omarchy-restart-shell` if
the bar keeps an older panel.
