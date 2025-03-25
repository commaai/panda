import os
import subprocess
import glob

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
      f"{panda_root}/../opendbc/safety/",
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

  # Compile startup file
  startup = env.Object(f"obj/startup_{project_name}", project["STARTUP_FILE"])

  # Compile crypto files
  crypto_obj = [
      env.Object(f"rsa-{project_name}", f"{panda_root}/crypto/rsa.c"),
      env.Object(f"sha-{project_name}", f"{panda_root}/crypto/sha.c")
  ]

  # Add necessary sources for bootstub
  bootstub_sources = [
      f"{panda_root}/board/bootstub.c",
      # [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/*.c")]
      f"{panda_root}/board/libc.c",# TODO: Add other sources to the build
      # Add other required source files here
  ]
  # Compile bootstub
  # Compile bootstub sources
  # bootstub_objs = [env.Object(f"bootstub-{project_name}-{i}", src) for i, src in enumerate(bootstub_sources)]
  # # Link bootstub ELF
  # bootstub_elf = env.Program(f"obj/bootstub.{project_name}.elf", [startup] + crypto_obj + bootstub_objs)
  # env.Objcopy(f"obj/bootstub.{project_name}.bin", bootstub_elf)

  # Compile all main source files into object files
  # main_objs = [env.Object(f"obj/{os.path.splitext(src)[0].replace('/', '_')}_{project_name}.o", src)
  #             for src in project["MAIN"]]

  main_objs = []
  for src in project["MAIN"]:
    # Generate a unique object file name based on the relative path
    obj_name = f"obj/{src.replace('/', '_').replace('.c', '')}_{project_name}.o"
    # Compile the source file
    main_objs.append(env.Object(obj_name, src))
  # Link all object files into the final binary
  main_elf = env.Program(f"obj/{project_name}.elf", [startup] + main_objs,
                          LINKFLAGS=[f"-Wl,--section-start,.isr_vector={project['APP_START_ADDRESS']}"] + flags)
  main_bin = env.Objcopy(f"obj/{project_name}.bin", main_elf)

  # Sign main binary
  sign_py = File(f"{panda_root}/crypto/sign.py").srcnode().relpath
  env.Command(f"obj/{project_name}.bin.signed", main_bin, f"SETLEN=1 {sign_py} $SOURCE $TARGET {cert_fn}")

# TODO: Separate source files for jungle boards
source_files = (
    [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/boards/*.c")] +  # Correct path for board_black.c
    [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/drivers/*.c")] +
    # [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/jungle/stm32f4/*.c")] +  # Correct path for jungle STM32F4 files
    # [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/jungle/stm32h7/*.c")] +  # Correct path for jungle STM32H7 files
    [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/stm32f4/*.c")] +
    [os.path.relpath(f) for f in glob.glob(f"{panda_root}/board/stm32h7/*.c")])
source_files = [s.replace("board/", "", 1) for s in source_files]
# print(source_files)
# exit()
f4_exclusion_list = ['cuatro.c', 'grey.c', 'red.c', 'tres.c', 'fake_siren.c', 'fan.c', 'fdcan.c']
f4_source_files = [file for file in source_files if file.split('/')[-1] not in f4_exclusion_list]

base_project_f4 = {
  "MAIN": f4_source_files,
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

h7_exclusion_list = ['black.c', 'dos.c', 'uno.c', 'white.c', 'bxcan.c']
h7_source_files = [file for file in source_files if file.split('/')[-1] not in h7_exclusion_list]

base_project_h7 = {
  "MAIN": h7_source_files,
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

# # panda jungle fw
# SConscript('board/jungle/SConscript')

# test files
if GetOption('extras'):
  SConscript('tests/libpanda/SConscript')