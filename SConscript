import platform

# panda fw
SConscript('board/SConscript')

# test files
if GetOption('test'):
  CC = 'gcc'
  platform = platform.system()
  if platform == 'Darwin':
    # gcc installed by homebrew has version suffix (e.g. gcc-12) in order to be  
    # distinguishable from system one - which acts as a symlink to clang
    CC += '-12'
  Export('CC', 'platform')

  SConscript('tests/libpanda/SConscript')
