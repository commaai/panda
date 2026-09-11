import os
import hashlib
import base64
import opendbc
import subprocess

PREFIX = "arm-none-eabi-"
BUILDER = "DEV"

common_flags = []

if os.getenv("RELEASE"):
  BUILD_TYPE = "RELEASE"
  cert_fn = os.getenv("CERT")
  assert cert_fn is not None, 'No certificate file specified. Please set CERT env variable'
  assert os.path.exists(cert_fn), 'Certificate file not found. Please specify absolute path'
else:
  BUILD_TYPE = "DEBUG"
  cert_fn = File("./board/certs/debug").srcnode().relpath
  common_flags += ["-DALLOW_DEBUG"]

  if os.getenv("DEBUG"):
    common_flags += ["-DDEBUG"]

def objcopy(source, target, env, for_signature):
    return '$OBJCOPY -O binary %s %s' % (source[0], target[0])

def get_version(builder, build_type):
  try:
    git = subprocess.check_output(["git", "rev-parse", "--short=8", "HEAD"], encoding='utf8').strip()
  except subprocess.CalledProcessError:
    git = "unknown"
  return f"{builder}-{git}-{build_type}"

def get_key_header(name):
  public_fn = File(f'./board/certs/{name}.pub').srcnode().get_path()
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
    f"  .len = 0x20,",
    f"  .n0inv = {n0inv}U,",
    f"  .n = {to_c_uint32(n)},",
    f"  .rr = {to_c_uint32(rr)},",
    f"  .exponent = {e},",
    f"}};",
  ]
  return r

def to_c_uint32(x):
  nums = []
  for _ in range(0x20):
    nums.append(x % (2**32))
    x //= (2**32)
  return "{" + 'U,'.join(map(str, nums)) + "U}"


def firmware_sources(project_name):
  common = [
    "board/obj/gitversion.c", "board/main_state.c", "board/boards/boards.c",
    "board/libc.c", "board/utils.c", "board/crc.c", "board/early_init.c", "board/provision.c",
    "board/sys/critical.c", "board/sys/faults.c",
    *[f"board/drivers/{name}.c" for name in ("gpio", "registers", "interrupts", "timers", "led", "pwm", "usb", "spi")],
    *[f"board/stm32h7/{name}.c" for name in ("clock", "peripherals", "interrupt_handlers", "lladc", "llspi", "llusb")],
  ]
  if project_name == "panda_h7":
    common += [f"board/boards/{name}.c" for name in ("red", "tres", "cuatro", "unused_funcs")]
    common += ["board/stm32h7/board.c"]
    common += [f"board/drivers/{name}.c" for name in ("harness", "fan", "clock_source", "fake_siren")]
    common += [f"board/stm32h7/{name}.c" for name in ("llfan", "lli2c", "lldts", "sound")]
  elif project_name == "panda_jungle_h7":
    common += ["board/jungle/boards/board_v2.c", "board/jungle/stm32h7/board.c"]
  else:
    common += ["board/body/boards/board_body.c", "board/body/stm32h7/board.c"]
  app = ["board/can_comms.c", "board/safety.c", "board/drivers/can_common.c", "board/drivers/fdcan.c",
         "board/drivers/uart.c", "board/stm32h7/lluart.c", "board/stm32h7/llfdcan.c"]
  if project_name == "panda_h7":
    app += ["board/main_comms.c", "board/sys/power_saving.c", "board/drivers/bootkick.c", "board/drivers/simple_watchdog.c"]
  elif project_name == "panda_jungle_h7":
    app += ["board/jungle/main_comms.c"]
  else:
    app += ["board/body/can.c", "board/body/dotstar.c", "board/body/main_comms.c", "board/body/bldc/bldc.c",
            "board/body/bldc/BLDC_controller.c", "board/body/bldc/BLDC_controller_data.c"]
  return common, app


def build_project(project_name, project, main, extra_flags):
  project_dir = Dir(f'./board/obj/{project_name}/')

  flags = project["FLAGS"] + extra_flags + common_flags + [
    "-Wall",
    "-Wextra",
    "-Wstrict-prototypes",
    "-Werror",
    "-mlittle-endian",
    "-mthumb",
    "-nostdlib",
    "-fno-builtin",
    "-std=gnu11",
    "-fmax-errors=1",
    f"-T{File(project['LINKER_SCRIPT']).srcnode().relpath}",
    "-fsingle-precision-constant",
    "-Os",
    "-g",
  ]

  env = Environment(
    ENV=os.environ,
    CC=PREFIX + 'gcc',
    AS=PREFIX + 'gcc',
    OBJCOPY=PREFIX + 'objcopy',
    OBJDUMP=PREFIX + 'objdump',
    CFLAGS=flags,
    ASFLAGS=flags,
    LINKFLAGS=flags,
    CPPPATH=[Dir("./"), "./board/stm32h7/inc", opendbc.INCLUDE_PATH],
    ASCOM="$AS $ASFLAGS -o $TARGET -c $SOURCES",
    BUILDERS={
      'Objcopy': Builder(generator=objcopy, suffix='.bin', src_suffix='.elf')
    },
    tools=["default", "compilation_db"],
  )

  startup = env.Object(f"{project_dir}/startup.o", project["STARTUP_FILE"])
  common_sources, app_sources = firmware_sources(project_name)

  def objects(build_env, kind, sources):
    return [build_env.Object(f"{project_dir}/{kind}/{src.removeprefix('./').removeprefix('board/').removesuffix('.c')}.o", src) for src in sources]


  # Build bootstub
  bs_env = env.Clone()
  bs_env.Append(CFLAGS="-DBOOTSTUB", ASFLAGS="-DBOOTSTUB", LINKFLAGS="-DBOOTSTUB")
  bs_elf = bs_env.Program(f"{project_dir}/bootstub.elf", [
    startup,
    *objects(bs_env, "bootstub", common_sources + [
      "board/crypto/rsa.c", "board/crypto/sha.c", "board/bootstub.c", "board/bootstub_debug.c", "board/flasher.c", "board/stm32h7/llflash.c",
    ]),
  ])
  bs_env.Objcopy(f"./board/obj/bootstub.{project_name}.bin", bs_elf)

  # Build + sign main (aka app)
  main_elf = env.Program(f"{project_dir}/main.elf", [
    startup,
    *objects(env, "app", common_sources + app_sources + [main]),
  ], LINKFLAGS=[f"-Wl,--section-start,.isr_vector={project['APP_START_ADDRESS']}"] + flags)
  main_bin = env.Objcopy(f"{project_dir}/main.bin", main_elf)
  sign_py = File(f"./board/crypto/sign.py").srcnode().relpath
  env.Command(f"./board/obj/{project_name}.bin.signed", main_bin, f"SETLEN=1 {sign_py} $SOURCE $TARGET {cert_fn}")



base_project_h7 = {
  "STARTUP_FILE": "./board/stm32h7/startup_stm32h7x5xx.s",
  "LINKER_SCRIPT": "./board/stm32h7/stm32h7x5_flash.ld",
  "APP_START_ADDRESS": "0x8020000",
  "FLAGS": [
    "-mcpu=cortex-m7",
    "-mhard-float",
    "-DSTM32H7",
    "-DSTM32H725xx",
    "-Iboard/stm32h7/inc",
    "-mfpu=fpv5-d16",
  ],
}

# Common autogenerated includes
with open("board/obj/gitversion.h", "w") as f:
  version = get_version(BUILDER, BUILD_TYPE)
  f.write('#pragma once\n\n#include <stdint.h>\n\n')
  f.write(f'extern const uint8_t gitversion[{len(version)+1}];\n')
with open("board/obj/gitversion.c", "w") as f:
  f.write('#include <stdint.h>\n#include "board/obj/gitversion.h"\n')
  f.write(f'const uint8_t gitversion[{len(version)+1}] = "{version}";\n')

with open("board/obj/version", "w") as f:
  f.write(f'{get_version(BUILDER, BUILD_TYPE)}')

certs = [get_key_header(n) for n in ["debug", "release"]]
with open("board/obj/cert.h", "w") as f:
  for cert in certs:
    f.write("\n".join(cert) + "\n")

# Packet version defines: SHA hash of the struct header files
def version_hash(path):
  with open(path, "rb") as f:
    # Normalize line endings on Windows
    return int.from_bytes(hashlib.sha256(f.read().replace(b'\r', b'')).digest()[:4], 'little')
hh, ch, jh = version_hash("board/health.h"), version_hash(os.path.join(opendbc.INCLUDE_PATH, "opendbc/safety/can.h")), version_hash("board/jungle/jungle_health.h")
common_flags += [f"-DHEALTH_PACKET_VERSION=0x{hh:08X}U", f"-DCAN_PACKET_VERSION_HASH=0x{ch:08X}U",
                 f"-DJUNGLE_HEALTH_PACKET_VERSION=0x{jh:08X}U"]

# panda fw
build_project("panda_h7", base_project_h7, "./board/main.c", [])

# panda jungle fw
flags = [
  "-DPANDA_JUNGLE",
]
build_project("panda_jungle_h7", base_project_h7, "./board/jungle/main.c", flags)

# body fw
build_project("body_h7", base_project_h7, "./board/body/main.c", ["-DPANDA_BODY"])

# test files
SConscript('tests/libpanda/SConscript')
