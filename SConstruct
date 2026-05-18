import os

AddOption('--extras', dest='extras', action='store_true', default=False, help='Build extra targets (tests)')

env = Environment(
  COMPILATIONDB_USE_ABSPATH=True,
  tools=["default", "compilation_db"],
)

SetOption('num_jobs', max(1, int((os.cpu_count() or 1)-1)))

env.CompilationDatabase("compile_commands.json")

# panda fw & test files
SConscript('SConscript')
