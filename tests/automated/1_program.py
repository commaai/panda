import os
from panda import Panda

LEGACY = os.getenv("LEGACY") is not None

# must run first
def test_recover():
  # download the latest code
  assert(Panda.recover(legacy=LEGACY))

  # connect to the panda
  p = Panda()

def test_flash():
  p = Panda()
  p.flash()
