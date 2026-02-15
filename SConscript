import os
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
  cert_fn = File("./certs/debug").srcnode().relpath
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

  public_fn = File(f'./certs/{name}.pub').srcnode().get_path()
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


def build_project(project_name, project, extra_sources, extra_flags):
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
    OBJPREFIX=project_dir,
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

  startup = env.Object(project["STARTUP_FILE"])

  drivers = [
    "./board/can_comms.c",
    "./board/crc.c",
    "./board/critical.c",
    "./board/early_init.c",
    "./board/faults.c",
    "./board/flasher.c",
    "./board/libc.c",
    "./board/power_saving.c",
    "./board/provision.c",
    "./board/utils.c",
    "./board/boards/cuatro.c",
    "./board/boards/red.c",
    "./board/boards/tres.c",
    "./board/boards/unused_funcs.c",
    "./board/drivers/bootkick.c",
    "./board/drivers/can_common.c",
    "./board/drivers/clock_source.c",
    "./board/drivers/fan.c",
    "./board/drivers/fdcan.c",
    "./board/drivers/gpio.c",
    "./board/drivers/harness.c",
    "./board/drivers/interrupts.c",
    "./board/drivers/led.c",
    "./board/drivers/pwm.c",
    "./board/drivers/registers.c",
    "./board/drivers/simple_watchdog.c",
    "./board/drivers/spi.c",
    "./board/drivers/timers.c",
    "./board/drivers/uart.c",
    "./board/drivers/usb.c",
    "./board/drivers/fake_siren.c",
    "./board/stm32h7/clock.c",
    "./board/stm32h7/interrupt_handlers.c",
    "./board/stm32h7/lladc.c",
    "./board/stm32h7/llfan.c",
    "./board/stm32h7/llfdcan.c",
    "./board/stm32h7/llflash.c",
    "./board/stm32h7/lli2c.c",
    "./board/stm32h7/llspi.c",
    "./board/stm32h7/lluart.c",
    "./board/stm32h7/llusb.c",
    "./board/stm32h7/peripherals.c",
    "./board/stm32h7/sound.c",
  ]

  # Build bootstub
  bs_env = env.Clone()
  bs_env.Append(CFLAGS="-DBOOTSTUB", ASFLAGS="-DBOOTSTUB", LINKFLAGS="-DBOOTSTUB")
  bs_elf = bs_env.Program(f"{project_dir}/bootstub.elf", [
    startup,
    "./crypto/rsa.c",
    "./crypto/sha.c",
    "./board/bootstub.c",
    "./board/bootstub_declarations.c",
  ] + drivers + extra_sources)
  bs_env.Objcopy(f"./board/obj/bootstub.{project_name}.bin", bs_elf)

  # Build + sign main (aka app)
  main_elf = env.Program(f"{project_dir}/main.elf", [
    startup,
    "./board/main_definitions.c",
  ] + drivers + extra_sources, LINKFLAGS=[f"-Wl,--section-start,.isr_vector={project['APP_START_ADDRESS']}"] + flags)
  main_bin = env.Objcopy(f"{project_dir}/main.bin", main_elf)
  sign_py = File(f"./crypto/sign.py").srcnode().relpath
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
  f.write(f'extern const uint8_t gitversion[{len(version)+1}];\n')
  f.write(f'const uint8_t gitversion[{len(version)+1}] = "{version}";\n')

with open("board/obj/version", "w") as f:
  f.write(f'{get_version(BUILDER, BUILD_TYPE)}')

certs = [get_key_header(n) for n in ["debug", "release"]]
with open("board/obj/cert.h", "w") as f:
  for cert in certs:
    f.write("\n".join(cert) + "\n")

# panda fw
panda_sources = [
  "./board/main.c",
  "./board/main_comms.c",
  "./board/stm32h7/board.c",
]
build_project("panda_h7", base_project_h7, panda_sources, [])

# panda jungle fw
flags = [
  "-DPANDA_JUNGLE",
]
if os.getenv("FINAL_PROVISIONING"):
  flags += ["-DFINAL_PROVISIONING"]

jungle_sources = [
  "./board/jungle/main.c",
  "./board/jungle/main_comms.c",
  "./board/jungle/boards/board_v2.c",
  "./board/jungle/boards/board_declarations.c",
  "./board/jungle/stm32h7/board.c",
]
build_project("panda_jungle_h7", base_project_h7, jungle_sources, flags)

# body fw
body_sources = [
  "./board/body/main.c",
  "./board/body/can.c",
  "./board/body/main_comms.c",
  "./board/body/motor_common.c",
  "./board/body/motor_control.c",
  "./board/body/motor_encoder.c",
  "./board/body/boards/board_body.c",
  "./board/body/stm32h7/board.c",
]
build_project("body_h7", base_project_h7, body_sources, ["-DPANDA_BODY"])

# test files
if GetOption('extras'):
  SConscript('tests/libpanda/SConscript')
