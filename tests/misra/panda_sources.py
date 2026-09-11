#!/usr/bin/env python3
"""List the panda application translation units from the firmware build."""
import json
import pathlib
import shlex

ROOT = pathlib.Path(__file__).resolve().parents[2]


def panda_sources():
  with (ROOT / "compile_commands.json").open() as f:
    commands = json.load(f)
  sources = set()
  for entry in commands:
    flags = shlex.split(entry["command"])
    if "-DSTM32H7" in flags and not any(flag in flags for flag in ("-DBOOTSTUB", "-DPANDA_BODY", "-DPANDA_JUNGLE")):
      source = pathlib.Path(entry["file"])
      if source.suffix == ".c":
        sources.add(source.relative_to(ROOT).as_posix())
  assert "board/main.c" in sources
  return sorted(sources)


if __name__ == "__main__":
  print("\n".join(panda_sources()))
