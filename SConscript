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
    f"{panda_root}/../opendbc",
    f"{panda_root}/../opendbc/safety",
    f"{panda_root}/include/",
    f"{panda_root}/include/board/",
    f"{panda_root}/include/board/stm32h7",
    f"{panda_root}/include/board/stm32f4",
    f"{panda_root}/include/board/boards/",
    f"{panda_root}/include/board/drivers/",
    f"{panda_root}/include/board/jungle/",
    f"{panda_root}/include/board/jungle/boards/",
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

  def make_object(_env, name, path):
      return _env.Object(f"{name}-{project_name}", path)

  # Sources shared by all Panda variants
  sources = [
      #  bootstub_definitions, flasher,
      ("safety", f"{panda_root}/../opendbc/safety/main.c"),
      ("early_init", f"{panda_root}/board/early_init.c"),
      ("libc", f"{panda_root}/board/libc.c"),
      ("critical", f"{panda_root}/board/critical.c"),
      ("faults", f"{panda_root}/board/faults_panda.c"),
      ("drivers_clock_source", f"{panda_root}/board/drivers/clock_source.c"),
      ("drivers_gpio", f"{panda_root}/board/drivers/gpio.c"),
      ("drivers_interrupts", f"{panda_root}/board/drivers/interrupts.c"),
      ("drivers_registers", f"{panda_root}/board/drivers/registers.c"),
      ("drivers_spi", f"{panda_root}/board/drivers/spi.c"),
      ("drivers_timers", f"{panda_root}/board/drivers/timers.c"),
      ("drivers_usb", f"{panda_root}/board/drivers/usb.c"),
  ]

  _is_panda_jungle = "PANDA_JUNGLE" in " ".join(extra_flags)
  _is_stm32h7 = "DSTM32H7" in " ".join(project["PROJECT_FLAGS"])
  _is_stm32f4 = "DSTM32F4" in " ".join(project["PROJECT_FLAGS"])

  # Jungle does not get these drivers.
  if not _is_panda_jungle:
      sources.extend([
          ("drivers_harness", f"{panda_root}/board/drivers/harness.c"),
          ("drivers_simple_watchdog", f"{panda_root}/board/drivers/simple_watchdog.c"),
          ("drivers_fan", f"{panda_root}/board/drivers/fan.c"),
      ])

  if _is_stm32h7:
      sources.extend([
        ("stm32h7_peripherals", f"{panda_root}/board/stm32h7/peripherals.c"),
        ("stm32h7_llusb", f"{panda_root}/board/stm32h7/llusb.c"),
        ("stm32h7_clock", f"{panda_root}/board/stm32h7/clock.c"),
        ("stm32h7_lladc", f"{panda_root}/board/stm32h7/lladc.c"),
        ("stm32h7_lldac", f"{panda_root}/board/stm32h7/lldac.c"),
        ("stm32h7_llflash", f"{panda_root}/board/stm32h7/llflash.c"),
        ("stm32h7_lli2c", f"{panda_root}/board/stm32h7/lli2c.c"),
        ("stm32h7_llspi", f"{panda_root}/board/stm32h7/llspi.c"),
      ])
      if not _is_panda_jungle:
          sources.extend([
            ("stm32h7_llfan", f"{panda_root}/board/stm32h7/llfan.c"),
            ("stm32h7_sound", f"{panda_root}/board/stm32h7/sound.c"),
          ])


  if _is_stm32f4:
      sources.extend([
        ("stm32f4_peripherals", f"{panda_root}/board/stm32f4/peripherals.c"),
        ("stm32f4_llusb", f"{panda_root}/board/stm32f4/llusb.c"),
        ("stm32f4_clock", f"{panda_root}/board/stm32f4/clock.c"),
        ("stm32f4_lladc", f"{panda_root}/board/stm32f4/lladc.c"),
        ("stm32f4_llflash", f"{panda_root}/board/stm32f4/llflash.c"),
        ("stm32f4_llspi", f"{panda_root}/board/stm32f4/llspi.c"),
      ])
      if not _is_panda_jungle:
          sources.extend([
            ("stm32f4_llfan", f"{panda_root}/board/stm32f4/llfan.c"),
          ])

  # Create a bootstub-specific environment with -DBOOTSTUB flag
  bootstub_env = env.Clone()
  bootstub_env.Append(CFLAGS=["-DBOOTSTUB"])
  bootstub_env.Append(ASFLAGS=["-DBOOTSTUB"])
  bootstub_env.Append(LINKFLAGS=["-DBOOTSTUB"])

  # Use bootstub_env for all bootstub-related objects
  bootstub_definitions = (f"bootstub_definitions", f"{panda_root}/board/bootstub_definitions.c")
  flasher = (f"flasher", f"{panda_root}/board/flasher.c")

  # Recompile all sources with the bootstub environment for bootstub use
  bootstub_sources = [
    make_object(bootstub_env, f"bootstubx-{name}", path)
    for name, path in sources + [bootstub_definitions, flasher]]

  # Compile bootstub.c with bootstub environment
  bootstub_obj = bootstub_env.Object(f"bootstub-{project_name}", File(project.get("BOOTSTUB", f"{panda_root}/board/bootstub.c")))
  bootstub_elf = bootstub_env.Program(f"obj/bootstub.{project_name}.elf",
                                   [startup] + bootstub_sources + crypto_obj + [bootstub_obj])
  bootstub_env.Objcopy(f"obj/bootstub.{project_name}.bin", bootstub_elf)

  # Needed for full build.
  sources.extend( [
      ("can_comms", f"{panda_root}/board/can_comms.c"),
      ("drivers_can_common", f"{panda_root}/board/drivers/can_common_panda.c"),
      ("drivers_uart", f"{panda_root}/board/drivers/uart.c"),
      ("main_definitions", f"{panda_root}/board/main_definitions.c"),
  ])
  if _is_panda_jungle:
      sources.extend([
          ("jungle_main_comms", f"{panda_root}/board/jungle/main_comms.c"),
      ])
  else:
      sources.extend([
          ("power_saving", f"{panda_root}/board/power_saving.c"),
          ("main_comms", f"{panda_root}/board/main_comms.c"),
          ("drivers_bootkick", f"{panda_root}/board/drivers/bootkick.c"),
      ])
  if _is_stm32h7:
      sources.extend([
        ("drivers_fdcan", f"{panda_root}/board/drivers/fdcan.c"),
        ("stm32h7_llfdcan", f"{panda_root}/board/stm32h7/llfdcan.c"),
        ("stm32h7_lluart", f"{panda_root}/board/stm32h7/lluart.c"),
      ])
  if _is_stm32f4:
      sources.extend([
        ("drivers_bxcan", f"{panda_root}/board/drivers/bxcan.c"),
        ("stm32f4_llbxcan", f"{panda_root}/board/stm32f4/llbxcan.c"),
        ("stm32f4_lluart", f"{panda_root}/board/stm32f4/lluart.c"),
      ])

  # Build main
  main_sources = [make_object(env, name, path) for  name, path in sources]
  main_obj = env.Object(f"main-{project_name}", project["MAIN"])
  main_elf = env.Program(f"obj/{project_name}.elf", [startup, main_obj] + main_sources,
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
  f.write(f'static const uint8_t gitversion[{len(version)}] = "{version}";\n')

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
