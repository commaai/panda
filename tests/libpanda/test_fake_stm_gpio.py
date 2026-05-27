from panda.tests.libpanda import libpanda_py


GPIOB = 1
MODE_OUTPUT = 1


def test_fake_stm_tracks_gpio_output_and_mode():
  libpanda_py.libpanda.fake_stm_reset_gpio()

  gpio = libpanda_py.libpanda.fake_stm_get_gpio(GPIOB)
  assert gpio.ODR == 0
  assert gpio.MODER == 0

  libpanda_py.libpanda.fake_stm_set_gpio_output(GPIOB, 7, True)
  assert gpio.ODR & (1 << 7)
  assert ((gpio.MODER >> (7 * 2)) & 0b11) == MODE_OUTPUT

  libpanda_py.libpanda.fake_stm_set_gpio_output(GPIOB, 7, False)
  assert (gpio.ODR & (1 << 7)) == 0
  assert ((gpio.MODER >> (7 * 2)) & 0b11) == MODE_OUTPUT
