import os
import hashlib
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
  from Crypto.PublicKey import RSA

  public_fn = File(f'./board/certs/{name}.pub').srcnode().get_path()
  with open(public_fn) as f:
    rsa = RSA.importKey(f.read())
  assert(rsa.size_in_bits() == 1024)

  rr = pow(2**1024, 2, rsa.n)
  n0inv = 2**32 - pow(rsa.n, -1, 2**32)

  r = [
    f"RSAPublicKey {name}_rsa_key = {{",
    f"  .len = 0x20,",
    f"  .n0inv = {n0inv}U,",
    f"  .n = {to_c_uint32(rsa.n)},",
    f"  .rr = {to_c_uint32(rr)},",
    f"  .exponent = {rsa.e},",
    f"}};",
  ]
  return r

def to_c_uint32(x):
  nums = []
  for _ in range(0x20):
    nums.append(x % (2**32))
    x //= (2**32)
  return "{" + 'U,'.join(map(str, nums)) + "U}"

common_srcs = [
  "./board/main_definitions.c",
  "./board/utils.c",
  "./board/libc.c",
  "./board/crc.c",
  "./board/gitversion.c",
  "./board/safety_definitions.c",
  "./board/stm32h7/lladc.c",
  "./board/sys/critical.c",
  "./board/sys/faults.c",
  "./board/early_init.c",
  "./board/provision.c",
  "./board/drivers/can_common.c",
  "./board/drivers/gpio.c",
  "./board/drivers/led.c",
  "./board/drivers/pwm.c",
  "./board/drivers/simple_watchdog.c",
  "./board/drivers/uart.c",
  "./board/drivers/usb.c",
  "./board/drivers/spi.c",
  "./board/drivers/timers.c",
  "./board/drivers/fdcan.c",
  "./board/drivers/registers.c",
  "./board/drivers/interrupts.c",
  "./board/can_comms.c",
]

panda_srcs = [
  "./board/main_comms.c",
  "./board/safety_mode_wrapper.c",
  "./board/sys/power_saving.c",
  "./board/drivers/fan.c",
  "./board/drivers/bootkick.c",
  "./board/drivers/harness.c",
  "./board/drivers/fake_siren.c",
  "./board/stm32h7/sound.c",
  "./board/drivers/clock_source.c",
  "./board/stm32h7/llfan.c",
]

panda_board_srcs = [
  "./board/stm32h7/board.c",
  "./board/boards/red.c",
  "./board/boards/tres.c",
  "./board/boards/cuatro.c",
  "./board/boards/unused_funcs.c",
]

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

  def make_objs(srcs, environment, suffix=""):
    objs = []
    for s in srcs:
      # Create a unique object path: board/obj/panda_h7/board/utils.bootstub.o
      target_path = s.replace('.c', f'{suffix}.o').replace('.s', f'{suffix}.o')
      target = project_dir.File(target_path)
      objs.append(environment.Object(target, s))
    return objs

  startup = env.Object(project_dir.File(project["STARTUP_FILE"].replace('.s', '.o')), project["STARTUP_FILE"])

  # Build bootstub
  bs_env = env.Clone()
  bs_env.Append(CFLAGS="-DBOOTSTUB", ASFLAGS="-DBOOTSTUB", LINKFLAGS="-DBOOTSTUB")

  bs_srcs = [
    "./board/crypto/rsa.c",
    "./board/crypto/sha.c",
    "./board/bootstub.c",
    "./board/flasher.c",
    "./board/stm32h7/llflash.c",
    "./board/bootstub_declarations.c",
    "./board/main_definitions.c",
    "./board/utils.c",
    "./board/libc.c",
    "./board/crc.c",
    "./board/gitversion.c",
    "./board/sys/critical.c",
    "./board/drivers/gpio.c",
    "./board/stm32h7/clock.c",
    "./board/stm32h7/peripherals.c",
    "./board/drivers/registers.c",
    "./board/drivers/interrupts.c",
    "./board/drivers/led.c",
    "./board/drivers/pwm.c",
    "./board/drivers/uart.c",
    "./board/drivers/usb.c",
    "./board/drivers/spi.c",
    "./board/drivers/timers.c",
    "./board/stm32h7/llusb.c",
    "./board/stm32h7/llspi.c",
    "./board/stm32h7/lladc.c",
    "./board/early_init.c",
    "./board/provision.c",
  ]
  if project_name == "panda_h7":
    bs_srcs += panda_board_srcs
  elif project_name == "body_h7":
    bs_srcs += ["./board/body/stm32h7/board.c"]
  elif project_name == "panda_jungle_h7":
    bs_srcs += ["board/jungle/boards/main_definitions.c", "./board/jungle/boards/board_v2.c", "./board/jungle/stm32h7/board.c"]

  bs_elf = bs_env.Program(f"{project_dir}/bootstub.elf", [startup] + make_objs(bs_srcs, bs_env, suffix=".bootstub"))
  bs_env.Objcopy(f"./board/obj/bootstub.{project_name}.bin", bs_elf)

  # Build + sign main (aka app)
  main_srcs = [main] + common_srcs + project.get("SOURCES", [])
  main_elf = env.Program(f"{project_dir}/main.elf", [startup] + make_objs(main_srcs, env, suffix=".app"),
                         LINKFLAGS=[f"-Wl,--section-start,.isr_vector={project['APP_START_ADDRESS']}"] + flags)
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
  "SOURCES": [
    "./board/stm32h7/lluart.c",
    "./board/stm32h7/llusb.c",
    "./board/stm32h7/llspi.c",
    "./board/stm32h7/clock.c",
    "./board/stm32h7/llfdcan.c",
    "./board/stm32h7/peripherals.c",
    "./board/stm32h7/interrupt_handlers.c",
  ],
}

# Common autogenerated includes
if not os.path.exists("board/obj"):
  os.makedirs("board/obj")

version = get_version(BUILDER, BUILD_TYPE)
with open("board/obj/gitversion.h", "w") as f:
  f.write(f'extern const uint8_t gitversion[{len(version)+1}];\n')

with open("board/gitversion.c", "w") as f:
  f.write('#include <stdint.h>\n')
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
    return int.from_bytes(hashlib.sha256(f.read()).digest()[:4], 'little')
hh, ch, jh = version_hash("board/health.h"), version_hash(os.path.join(opendbc.INCLUDE_PATH, "opendbc/safety/can.h")), version_hash("board/jungle/jungle_health.h")
common_flags += [f"-DHEALTH_PACKET_VERSION=0x{hh:08X}U", f"-DCAN_PACKET_VERSION_HASH=0x{ch:08X}U",
                 f"-DJUNGLE_HEALTH_PACKET_VERSION=0x{jh:08X}U"]

# panda fw
panda_project_h7 = base_project_h7.copy()
panda_project_h7["SOURCES"] = base_project_h7["SOURCES"] + panda_srcs + panda_board_srcs
build_project("panda_h7", panda_project_h7, "./board/main.c", [])

# panda jungle fw
flags = [
  "-DPANDA_JUNGLE",
]
jungle_project_h7 = base_project_h7.copy()
jungle_project_h7["SOURCES"] = base_project_h7["SOURCES"] + ["board/jungle/boards/main_definitions.c", "./board/jungle/boards/board_v2.c", "./board/jungle/stm32h7/board.c"]
build_project("panda_jungle_h7", jungle_project_h7, "./board/jungle/main.c", flags)

# body fw
body_project_h7 = base_project_h7.copy()
body_project_h7["SOURCES"] = base_project_h7["SOURCES"] + ["board/body/main_definitions.c", "./board/body/motor_encoder.c", "./board/body/motor_control.c", "./board/body/boards/board_body.c", "./board/body/stm32h7/board.c"]
build_project("body_h7", body_project_h7, "./board/body/main.c", ["-DPANDA_BODY"])

# test files
SConscript('tests/libpanda/SConscript')
