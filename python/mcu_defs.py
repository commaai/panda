import os


BASEDIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../")

BOOTSTUB_ADDRESS = 0x8000000
APP_ADDRESS_FX = 0x8004000
APP_ADDRESS_H7 = 0x8020000

BLOCK_SIZE_FX = 0x800
BLOCK_SIZE_H7 = 0x400

DEFAULT_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda.bin.signed")
DEFAULT_H7_FW_FN = os.path.join(BASEDIR, "board", "obj", "panda_h7.bin.signed")
