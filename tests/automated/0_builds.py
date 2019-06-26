import os
from panda import build_st

def test_build_panda():
  build_error = build_st("obj/panda.bin")
  if bool(build_error):
    raise Exception(build_error)

def test_build_bootstub_panda():
  build_error = build_st("obj/bootstub.panda.bin")
  if bool(build_error):
    raise Exception(build_error)
