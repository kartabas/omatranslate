---
name: omatranslate
description: |
  Translate passages into another language with Omatranslate, the PDF reading
  companion, or read the app's session so their translations can be exported.
  Use for ANY request to translate text, read a foreign-language PDF, or
  produce a Markdown file of translations.
---

# Omatranslate

Omatranslate translates text through Google Translate and keeps the
translations of one reading session, to export as Markdown beside the PDF.
The PDF itself is never changed. The commands below need no display.

## Translating

```sh
omatranslate translate --to de "The cat sat on the mat."
omatranslate translate --to de --from en - < passage.txt
omatranslate translate --to pt --json "Good morning"
```

`--to` is required. `--from` defaults to `auto`, which asks Google to detect
the language; the JSON answer reports it as `detected`. `languages --json`
lists every code the service accepts. Each command exits 0 on success and 1
on failure, with the error on stderr.

The person picked Google Translate as the service, so the text you send
leaves the machine. Send only the passage they asked about, never a whole
document.

## The app

`omatranslate open <pdf>` opens the PDF in the person's reader. When they
highlight a passage there, a Translate chip appears beside it; clicking it
opens the translation in a card over the page, and the passage joins the
session in Omatranslate's window. That needs their desktop, so do not wait on
it. Their translations live only in that window for the session; the app
exports them with Ctrl+E or the Export button as `<pdf-name>.omatranslate.md`.

## Installing this skill

`omatranslate skill` prints this text. `omatranslate skill install` copies it
to `~/.agents/skills/omatranslate` and links it into `~/.claude/skills` when
that directory exists.
