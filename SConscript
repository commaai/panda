import os
import subprocess


PREFIX = "arm-none-eabi-"
BUILDER = "DEV"

common_flags = []

panda_root = Dir('.')

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


def build_project(project_name, project, extra_flags):
  linkerscript_fn = File(project["LINKER_SCRIPT"]).srcnode().relpath

  flags = project["PROJECT_FLAGS"] + extra_flags + common_flags + [
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
    f"-T{linkerscript_fn}",
  ]

  includes = [
    '.',
    '..',
    panda_root,
    f"{panda_root}/board/",
    f"{panda_root}/include/",
    f"{panda_root}/include/board/",
    f"{panda_root}/include/board/drivers/",
    f"{panda_root}/include/board/jungle/",
    f"{panda_root}/include/board/jungle/boards/",
    #f"{panda_root}/../opendbc/safety/",
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
    CPPPATH=includes,
    ASCOM="$AS $ASFLAGS -o $TARGET -c $SOURCES",
    BUILDERS={
      'Objcopy': Builder(generator=objcopy, suffix='.bin', src_suffix='.elf')
    },
    tools=["default", "compilation_db"],
  )

  startup = env.Object(f"obj/startup_{project_name}", project["STARTUP_FILE"])

  # Bootstub
  crypto_obj = [
    env.Object(f"rsa-{project_name}", f"{panda_root}/crypto/rsa.c"),
    env.Object(f"sha-{project_name}", f"{panda_root}/crypto/sha.c")
  ]

  # Sources shared by all Panda variants
  sources = [
      env.Object(f"safety2-{project_name}", f"{panda_root}/board/safety2.c"),
      env.Object(f"can-{project_name}", f"{panda_root}/board/can.c"),
      env.Object(f"can_comms-{project_name}", f"{panda_root}/board/can_comms.c"),
      env.Object(f"critical-{project_name}", f"{panda_root}/board/critical.c"),
      env.Object(f"faults-{project_name}", f"{panda_root}/board/faults.c"),
      env.Object(f"drivers_can_common-{project_name}", f"{panda_root}/board/drivers/can_common.c"),
      env.Object(f"drivers_clock_source-{project_name}", f"{panda_root}/board/drivers/clock_source.c"),
      env.Object(f"drivers_gpio-{project_name}", f"{panda_root}/board/drivers/gpio.c"),
      env.Object(f"drivers_interrupts-{project_name}", f"{panda_root}/board/drivers/interrupts.c"),
      env.Object(f"drivers_registers-{project_name}", f"{panda_root}/board/drivers/registers.c"),
      env.Object(f"drivers_simple_watchdog-{project_name}", f"{panda_root}/board/drivers/simple_watchdog.c"),
      env.Object(f"drivers_spi-{project_name}", f"{panda_root}/board/drivers/spi.c"),
      env.Object(f"drivers_timers-{project_name}", f"{panda_root}/board/drivers/timers.c"),
      env.Object(f"drivers_uart-{project_name}", f"{panda_root}/board/drivers/uart.c"),
      env.Object(f"drivers_usb-{project_name}", f"{panda_root}/board/drivers/usb.c"),
  ]

  # Jungle board does not get these drivers.
  if "PANDA_JUNGLE" not in " ".join(extra_flags):
      sources.extend([
          env.Object(f"drivers_fan-{project_name}", f"{panda_root}/board/drivers/fan.c"),
          env.Object(f"power_saving-{project_name}", f"{panda_root}/board/power_saving.c"),
          env.Object(f"drivers_bootkick-{project_name}", f"{panda_root}/board/drivers/bootkick.c"),
          env.Object(f"drivers_harness-{project_name}", f"{panda_root}/board/drivers/harness.c"),
      ])

  bootstub_obj = env.Object(f"bootstub-{project_name}", File(project.get("BOOTSTUB", f"{panda_root}/board/bootstub.c")))
  bootstub_elf = env.Program(f"obj/bootstub.{project_name}.elf",
                                     [startup] + crypto_obj + sources + [bootstub_obj])
  env.Objcopy(f"obj/bootstub.{project_name}.bin", bootstub_elf)


  # Add some more sources that we skip for bootstub.
  if "DSTM32H7" in " ".join(project["PROJECT_FLAGS"]):
      sources.append(env.Object(f"drivers_fdcan-{project_name}", f"{panda_root}/board/drivers/fdcan.c"))
  if "DSTM32F4" in " ".join(project["PROJECT_FLAGS"]):
      sources.append(env.Object(f"drivers_bxcan-{project_name}", f"{panda_root}/board/drivers/bxcan.c"))
  # Build main
  main_obj = env.Object(f"main-{project_name}", project["MAIN"])
  main_elf = env.Program(f"obj/{project_name}.elf", [startup, main_obj] + sources,
    LINKFLAGS=[f"-Wl,--section-start,.isr_vector={project['APP_START_ADDRESS']}"] + flags)
  main_bin = env.Objcopy(f"obj/{project_name}.bin", main_elf)

  # Sign main
  sign_py = File(f"{panda_root}/crypto/sign.py").srcnode().relpath
  env.Command(f"obj/{project_name}.bin.signed", main_bin, f"SETLEN=1 {sign_py} $SOURCE $TARGET {cert_fn}")


base_project_f4 = {
  "MAIN": "main.c",
  "STARTUP_FILE": File("./board/stm32f4/startup_stm32f413xx.s"),
  "LINKER_SCRIPT": File("./board/stm32f4/stm32f4_flash.ld"),
  "APP_START_ADDRESS": "0x8004000",
  "PROJECT_FLAGS": [
    "-mcpu=cortex-m4",
    "-mhard-float",
    "-DSTM32F4",
    "-DSTM32F413xx",
    "-Iboard/stm32f4/inc",
    "-mfpu=fpv4-sp-d16",
    "-fsingle-precision-constant",
    "-Os",
    "-g",
  ],
}

base_project_h7 = {
  "MAIN": "main.c",
  "STARTUP_FILE": File("./board/stm32h7/startup_stm32h7x5xx.s"),
  "LINKER_SCRIPT": File("./board/stm32h7/stm32h7x5_flash.ld"),
  "APP_START_ADDRESS": "0x8020000",
  "PROJECT_FLAGS": [
    "-mcpu=cortex-m7",
    "-mhard-float",
    "-DSTM32H7",
    "-DSTM32H725xx",
    "-Iboard/stm32h7/inc",
    "-mfpu=fpv5-d16",
    "-fsingle-precision-constant",
    "-Os",
    "-g",
  ],
}

Export('base_project_f4', 'base_project_h7', 'build_project')


# Common autogenerated includes
with open("board/obj/gitversion.h", "w") as f:
  version = get_version(BUILDER, BUILD_TYPE)
  f.write(f'extern const uint8_t gitversion[{len(version)}];\n')
  f.write(f'const uint8_t gitversion[{len(version)}] = "{version}";\n')

with open("board/obj/version", "w") as f:
  f.write(f'{get_version(BUILDER, BUILD_TYPE)}')

certs = [get_key_header(n) for n in ["debug", "release"]]
with open("board/obj/cert.h", "w") as f:
  for cert in certs:
    f.write("\n".join(cert) + "\n")

# panda fw
SConscript('board/SConscript')

# panda jungle fw
SConscript('board/jungle/SConscript')

# test files
if GetOption('extras'):
  SConscript('tests/libpanda/SConscript')
