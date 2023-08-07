import os
import time
import pytest

from panda import Panda, PandaDFU, McuType, BASEDIR

def test1(p):
  print("1", p.get_type(), p.health()['uptime'])

def test2(p):
  print("2", p.get_type(), p.health()['uptime'])

def test3(p):
  print("3", p.get_type(), p.health()['uptime'])
