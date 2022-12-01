import os

TICI = os.path.isfile('/TICI')

AddOption('--test',
          action='store_true',
          default=not TICI,
          help='build test files')

# panda fw & test files
SConscript('SConscript')
