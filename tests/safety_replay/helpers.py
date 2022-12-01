#!/usr/bin/env python3
import panda.tests.libpanda.libpanda_py as libpanda_py
from panda import Panda

def to_signed(d, bits):
  ret = d
  if d >= (1 << (bits - 1)):
    ret = d - (1 << bits)
  return ret

def is_steering_msg(mode, addr):
  ret = False
  if mode in (Panda.SAFETY_HONDA_NIDEC, Panda.SAFETY_HONDA_BOSCH):
    ret = (addr == 0xE4) or (addr == 0x194) or (addr == 0x33D) or (addr == 0x33DA) or (addr == 0x33DB)
  elif mode == Panda.SAFETY_TOYOTA:
    ret = addr == 0x2E4
  elif mode == Panda.SAFETY_GM:
    ret = addr == 384
  elif mode == Panda.SAFETY_HYUNDAI:
    ret = addr == 832
  elif mode == Panda.SAFETY_CHRYSLER:
    ret = addr == 0x292
  elif mode == Panda.SAFETY_SUBARU:
    ret = addr == 0x122
  return ret

def get_steer_torque(mode, to_send):
  ret = 0
  if mode in (Panda.SAFETY_HONDA_NIDEC, Panda.SAFETY_HONDA_BOSCH):
    ret = to_send.RDLR & 0xFFFF0000
  elif mode == Panda.SAFETY_TOYOTA:
    ret = (to_send.RDLR & 0xFF00) | ((to_send.RDLR >> 16) & 0xFF)
    ret = to_signed(ret, 16)
  elif mode == Panda.SAFETY_GM:
    ret = ((to_send.data[0] & 0x7) << 8) | to_send.data[1]
    ret = to_signed(ret, 11)
  elif mode == Panda.SAFETY_HYUNDAI:
    ret = (((to_send.data[3] & 0x7) << 8) | to_send.data[2]) - 1024
  elif mode == Panda.SAFETY_CHRYSLER:
    ret = ((to_send.RDLR & 0x7) << 8) + ((to_send.RDLR & 0xFF00) >> 8) - 1024
  elif mode == Panda.SAFETY_SUBARU:
    ret = ((to_send.RDLR >> 16) & 0x1FFF)
    ret = to_signed(ret, 13)
  return ret

def package_can_msg(msg):
  return libpanda_py.make_CANPacket(msg.address, msg.src, msg.dat)

def init_segment(safety, lr, mode):
  sendcan = (msg for msg in lr if msg.which() == 'sendcan')
  steering_msgs = (can for msg in sendcan for can in msg.sendcan if is_steering_msg(mode, can.address))

  msg = next(steering_msgs, None)
  if msg is None:
    # no steering msgs
    return

  to_send = package_can_msg(msg)
  torque = get_steer_torque(mode, to_send)
  if torque != 0:
    safety.set_controls_allowed(1)
    safety.set_desired_torque_last(torque)
    assert safety.safety_tx_hook(to_send), "failed to initialize panda safety for segment"
