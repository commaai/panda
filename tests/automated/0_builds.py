import os

BASEDIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../")
os.chdir(os.path.join(BASEDIR, "../board"))

def build(target, mkfile="Makefile"):
  assert(os.system('make -f %s clean && make -f %s %s >/dev/null' % (mkfile, mkfile, target)) == 0)

def test_build_legacy():
  build("obj/comma.bin", "Makefile.legacy")

def test_build_bootstub_legacy():
  build("obj/bootstub.comma.bin", "Makefile.legacy")

def test_build_panda():
  build("obj/panda.bin")

def test_build_bootstub_panda():
  build("obj/bootstub.panda.bin")

