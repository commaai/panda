AddOption('--minimal',
          action='store_false',
          dest='extras',
          default=True,
          help='the minimum build. no tests, tools, etc.')

AddOption('--ubsan',
          action='store_true',
          help='turn on UBSan')

AddOption('--coverage',
          action='store_true',
          help='build with test coverage options')

AddOption('--compile_db',
          action='store_true',
          help='build clang compilation database')

env = Environment(
  COMPILATIONDB_USE_ABSPATH=True,
  tools=["default", "compilation_db"],
)
  
if GetOption('compile_db'):
    # whole project compilation database
    env.CompilationDatabase("compile_commands.json")

    # Panda compilation database
    env_p = env.Clone()
    env_p["COMPILATIONDB_PATH_FILTER"] = '*board/[!jungle/][!bootstub]*'
    env_p.CompilationDatabase("compile_commands_panda.json")

# panda fw & test files
SConscript('SConscript')
