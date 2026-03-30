#!/usr/bin/env python3
"""Verify wheel contents: required firmware present, no intermediate artifacts."""
import sys
import zipfile

REQUIRED = {
    "panda/board/obj/panda_h7.bin.signed",
    "panda/board/obj/bootstub.panda_h7.bin",
    "panda/board/obj/panda_jungle_h7.bin.signed",
    "panda/board/obj/bootstub.panda_jungle_h7.bin",
    "panda/board/obj/body_h7.bin.signed",
    "panda/board/obj/bootstub.body_h7.bin",
    "panda/board/certs/debug",
    "panda/board/certs/debug.pub",
    "panda/board/certs/release.pub",
}

FORBIDDEN_SUFFIXES = (".o", ".d", ".elf", ".map")


def check_wheel(wheel_path):
    errors = []
    with zipfile.ZipFile(wheel_path) as zf:
        names = set(zf.namelist())

        missing = REQUIRED - names
        if missing:
            errors.append(f"Missing required files:\n  " + "\n  ".join(sorted(missing)))

        forbidden = [n for n in names if n.endswith(FORBIDDEN_SUFFIXES)]
        if forbidden:
            errors.append(f"Forbidden intermediate files:\n  " + "\n  ".join(sorted(forbidden)))

    if errors:
        print("FAIL:")
        for e in errors:
            print(e)
        return 1

    print(f"PASS: {wheel_path}")
    print(f"  Required firmware: {len(REQUIRED)} files present")
    print(f"  No forbidden intermediates")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <wheel_path>")
        sys.exit(1)
    sys.exit(check_wheel(sys.argv[1]))
