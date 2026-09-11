#!/usr/bin/env python3
"""Generate firmware metadata and record compiler commands for Make."""
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def write_if_changed(path, contents):
  path = Path(path)
  path.parent.mkdir(parents=True, exist_ok=True)
  if not path.exists() or path.read_text() != contents:
    path.write_text(contents)


def get_key_header(name):
  public_fn = f"board/certs/{name}.pub"
  with open(public_fn, "rb") as f:
    key = base64.b64decode(f.read().split()[1])
  values = []
  for _ in range(3):
    length = int.from_bytes(key[:4], "big")
    values.append(key[4:4 + length])
    key = key[4 + length:]
  _, e, n = values
  e, n = int.from_bytes(e, "big"), int.from_bytes(n, "big")
  assert n.bit_length() == 1024

  rr = pow(2**1024, 2, n)
  n0inv = 2**32 - pow(n, -1, 2**32)

  r = [
    f"RSAPublicKey {name}_rsa_key = {{",
    "  .len = 0x20,",
    f"  .n0inv = {n0inv}U,",
    f"  .n = {to_c_uint32(n)},",
    f"  .rr = {to_c_uint32(rr)},",
    f"  .exponent = {e},",
    "};",
  ]
  return r

def to_c_uint32(x):
  nums = []
  for _ in range(0x20):
    nums.append(x % (2**32))
    x //= (2**32)
  return "{" + 'U,'.join(map(str, nums)) + "U}"


def generate(build_type, include_path):
  try:
    revision = subprocess.check_output(["git", "rev-parse", "--short=8", "HEAD"], text=True).strip()
  except subprocess.CalledProcessError:
    revision = "unknown"
  version = f"DEV-{revision}-{build_type}"
  write_if_changed("board/obj/version", version)
  write_if_changed("board/obj/gitversion.h",
                   f'extern const uint8_t gitversion[{len(version)+1}];\n'
                   + f'const uint8_t gitversion[{len(version)+1}] = "{version}";\n')
  write_if_changed("board/obj/cert.h", "".join("\n".join(get_key_header(n)) + "\n" for n in ["debug", "release"]))
  headers = {
    "HEALTH_PACKET_VERSION": Path("board/health.h"),
    "CAN_PACKET_VERSION_HASH": Path(include_path) / "opendbc/safety/can.h",
    "JUNGLE_HEALTH_PACKET_VERSION": Path("board/jungle/jungle_health.h"),
  }
  defines = []
  for name, path in headers.items():
    value = int.from_bytes(hashlib.sha256(path.read_bytes().replace(b"\r", b"")).digest()[:4], "little")
    defines.append(f"#define {name} 0x{value:08X}U\n")
  write_if_changed("board/obj/packet_versions.h", "".join(defines))


if __name__ == "__main__":
  action, *args = sys.argv[1:]
  if action == "generate":
    generate(*args)
  elif action == "config":
    write_if_changed(args[0], "\n".join(args[1:]))
  elif action == "compile":
    output, source, *command = args
    result = subprocess.run(command)
    if result.returncode:
      sys.exit(result.returncode)
    entry = {"directory": os.getcwd(), "file": str(Path(source).resolve()), "output": str(Path(output).resolve()), "arguments": command}
    write_if_changed(output + ".json", json.dumps(entry))
  elif action == "database":
    write_if_changed("compile_commands.json", json.dumps([json.loads(Path(p + ".json").read_text()) for p in args], indent=2) + "\n")
