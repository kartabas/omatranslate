# Contributing

Omatranslate is a small, focused tool: translate what you highlight in a PDF,
without changing the PDF or translating anything by itself. Changes that keep
that promise are welcome.

## Before you start

- Open an issue for anything larger than a fix, so the shape can be agreed
  first.
- Read [Architecture](docs/architecture.md) — the single-process model and the
  control socket explain most of the app's behaviour.
- Check [Troubleshooting](docs/troubleshooting.md); it covers the common
  environment problems (offscreen Qt, missing `wl-clipboard`, stale bar
  widget).

## Making a change

```sh
git clone https://github.com/kartabas/omatranslate
cd omatranslate
./bin/build
./bin/test
```

1. Add a test that fails for the bug, or that describes the new behaviour.
2. Make the change as small as it can be.
3. Run `./bin/test`; both the Qt suite and the CLI checks must pass.
4. Update the docs when behaviour, settings, or commands change. The README is
   the overview; `docs/` is the reference.
5. Commit with a sentence that says what changed, in the imperative:
   `Keep the window hidden until it is asked for`.

`./bin/install` needs `sudo` (it calls `makepkg -i`), so it is not needed for
ordinary development; `./bin/build` plus `build/omatranslate open` is enough.

## What belongs here

- The PDF is never modified; export writes a sidecar file.
- Highlighting never translates automatically. Do not add an auto-translate
  mode.
- The app must stay usable without a mouse: new controls need a shortcut or a
  keyboard path.
- New user-visible text goes through `tr()`/`qsTr()`, and text that comes from
  a document or a provider uses `Text.PlainText`.
- Follow the Omarchy UI kit (`qs.Ui`) instead of drawing new buttons, panels,
  or focus rings.

## Translations

Interface catalogs are plain JSON keyed by the English string:

```
src/i18n/en.json          the window
src/omarchy/i18n/en.json  the bar widget
```

Copy an existing file, translate the values, add it to `src/resources.qrc`
(and the widget manifest where needed), and run the app once in that language
to check for truncation. Keep the keys unchanged.

## Reporting a bug

Include the output of:

```sh
omatranslate --version
omatranslate status --json
journalctl --user -n 200 | grep -i omatranslate
```

and say what you did, what you expected, and what happened. A screenshot of
the card or the bar widget helps when the problem is visual.

## License

By contributing, you agree that your work is released under the MIT license
in [LICENSE](LICENSE).
