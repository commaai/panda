AddOption('--minimal',
          action='store_false',
          dest='extras',
          default=True,
          help='the minimum build. no tests, tools, etc.')

AddOption('--ubsan',
          action='store_true',
          help='turn on UBSan')

AddOption('--compile_db',
          action='store_true',
          help='build clang compilation database')

AddOption('--mutation',
          action='store_true',
          help='generate mutation-ready code')

env = Environment(
  COMPILATIONDB_USE_ABSPATH=True,
  tools=["default", "compilation_db"],
)
  
if GetOption('compile_db'):
    env.CompilationDatabase("compile_commands.json")

# panda fw & test files
SConscript('SConscript')
