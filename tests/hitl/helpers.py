import time
import random


def time_many_sends(p, bus, p_recv=None, msg_count=100, msg_id=None, two_pandas=False):
  if p_recv is None:
    p_recv = p
  if msg_id is None:
    msg_id = random.randint(0x100, 0x200)
  if p == p_recv and two_pandas:
    raise ValueError("Cannot have two pandas that are the same panda")

  start_time = time.monotonic()
  p.can_send_many([(msg_id, 0, b"\xaa" * 8, bus)] * msg_count)
  r = []
  r_echo = []
  r_len_expected = msg_count if two_pandas else msg_count * 2
  r_echo_len_exected = msg_count if two_pandas else 0

  while len(r) < r_len_expected and (time.monotonic() - start_time) < 5:
    r.extend(p_recv.can_recv())
  end_time = time.monotonic()
  if two_pandas:
    while len(r_echo) < r_echo_len_exected and (time.monotonic() - start_time) < 10:
      r_echo.extend(p.can_recv())

  sent_echo = [x for x in r if x[3] == 0x80 | bus and x[0] == msg_id]
  sent_echo.extend([x for x in r_echo if x[3] == 0x80 | bus and x[0] == msg_id])
  resp = [x for x in r if x[3] == bus and x[0] == msg_id]

  leftovers = [x for x in r if (x[3] != 0x80 | bus and x[3] != bus) or x[0] != msg_id]
  assert len(leftovers) == 0

  assert len(resp) == msg_count
  assert len(sent_echo) == msg_count

  end_time = (end_time - start_time) * 1000.0
  comp_kbps = (1 + 11 + 1 + 1 + 1 + 4 + 8 * 8 + 15 + 1 + 1 + 1 + 7) * msg_count / end_time

  return comp_kbps


def clear_can_buffers(panda):
  # clear tx buffers
  for i in range(4):
    panda.can_clear(i)

  # clear rx buffers
  panda.can_clear(0xFFFF)
  r = [1]
  st = time.monotonic()
  while len(r) > 0:
    r = panda.can_recv()
    time.sleep(0.05)
    if (time.monotonic() - st) > 10:
      print("Unable to clear can buffers for panda ", panda.get_serial())
      assert False
