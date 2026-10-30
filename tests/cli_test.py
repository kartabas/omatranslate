#!/usr/bin/env python3
"""Checks the commands that need no display, and no network."""

import json
import os
import subprocess
import sys

BIN = sys.argv[1]
failures = []


def run(*args):
    env = {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "HOME": os.environ.get("HOME", "/tmp"),
        "LANG": "C.UTF-8",
    }
    return subprocess.run([BIN, *args], capture_output=True, text=True, env=env, timeout=60)


def check(name, condition, detail=""):
    if condition:
        print(f"PASS {name}")
    else:
        print(f"FAIL {name}{': ' + detail if detail else ''}")
        failures.append(name)


help_result = run("help")
check("help exits 0", help_result.returncode == 0, help_result.stderr)
check("help lists translate", "translate" in help_result.stdout)

bare = run()
check("bare command prints help", bare.returncode == 0 and "Usage: omatranslate" in bare.stdout)

version = run("--version")
check("version", version.returncode == 0 and "omatranslate 0.1.0" in version.stdout)

languages = run("languages", "--json")
try:
    codes = {entry["code"] for entry in json.loads(languages.stdout)["languages"]}
except Exception as error:  # noqa: BLE001 - report whatever went wrong
    codes = set()
    check("languages --json parses", False, str(error))
else:
    check("languages --json parses", True)
check("languages include auto, en, de and da", {"auto", "en", "de", "da"} <= codes)

plain = run("languages")
check("languages prints a table", plain.returncode == 0 and "en\tEnglish" in plain.stdout)

no_target = run("translate", "Hello")
check(
    "translate wants a target",
    no_target.returncode == 1 and "target language" in no_target.stderr,
    no_target.stderr,
)

unknown_target = run("translate", "--to", "klingon", "Hello")
check(
    "translate rejects an unknown target",
    unknown_target.returncode == 1 and "Unknown target language" in unknown_target.stderr,
    unknown_target.stderr,
)

unknown_source = run("translate", "--from", "klingon", "--to", "de", "Hello")
check(
    "translate rejects an unknown source",
    unknown_source.returncode == 1 and "Unknown source language" in unknown_source.stderr,
    unknown_source.stderr,
)

empty = run("translate", "--to", "de", "")
check(
    "translate rejects an empty passage",
    empty.returncode == 1 and "Nothing to translate" in empty.stderr,
    empty.stderr,
)

skill = run("skill")
check("skill prints the skill", skill.returncode == 0 and "omatranslate translate" in skill.stdout)

skill_json = run("skill", "--json")
try:
    parsed = json.loads(skill_json.stdout)
except Exception as error:  # noqa: BLE001
    check("skill --json parses", False, str(error))
else:
    check("skill --json parses", "omatranslate translate" in parsed.get("skill", ""))

translate_help = run("help", "translate")
check(
    "help translate explains its options",
    translate_help.returncode == 0 and "--to" in translate_help.stdout,
)

if failures:
    print()
    print("Failed: " + ", ".join(failures))
    sys.exit(1)
print()
print("All command line checks passed.")
