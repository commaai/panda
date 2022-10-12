class Buttons:
  NONE = 0
  RESUME = 1
  SET = 2
  CANCEL = 4

PREV_BUTTON_SAMPLES = 8
ENABLE_BUTTONS = (Buttons.RESUME, Buttons.SET, Buttons.CANCEL)


class HyundaiButtonBase: #(common.PandaSafetyTest):
  # pylint: disable=no-member,abstract-method
  BUTTONS_BUS = 0  # tx on this bus, rx on 0. added to all `self._tx(self._button_msg(...))`
  SCC_BUS = 0  # rx on this bus

  def test_button_sends(self):
    """
      Only RES and CANCEL buttons are allowed
      - RES allowed while controls allowed
      - CANCEL allowed while cruise is enabled
    """
    self.safety.set_controls_allowed(0)
    self.assertFalse(self._tx(self._button_msg(Buttons.RESUME, bus=self.BUTTONS_BUS)))
    self.assertFalse(self._tx(self._button_msg(Buttons.SET, bus=self.BUTTONS_BUS)))

    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._button_msg(Buttons.RESUME, bus=self.BUTTONS_BUS)))
    self.assertFalse(self._tx(self._button_msg(Buttons.SET, bus=self.BUTTONS_BUS)))

    for enabled in (True, False):
      self._rx(self._pcm_status_msg(enabled))
      self.assertEqual(enabled, self._tx(self._button_msg(Buttons.CANCEL, bus=self.BUTTONS_BUS)))

  def test_enable_control_allowed_from_cruise(self):
    """
      Hyundai non-longitudinal only enables on PCM rising edge and recent button press. Tests PCM enabling with:
      - disallowed: No buttons
      - disallowed: Buttons that don't enable cruise
      - allowed: Buttons that do enable cruise
      - allowed: Main button with all above combinations
    """
    for main_button in (0, 1):
      for btn in range(8):
        for _ in range(PREV_BUTTON_SAMPLES):  # reset
          self._rx(self._button_msg(Buttons.NONE))

        self._rx(self._pcm_status_msg(False))
        self.assertFalse(self.safety.get_controls_allowed())
        self._rx(self._button_msg(btn, main_button=main_button))
        self._rx(self._pcm_status_msg(True))
        controls_allowed = btn in ENABLE_BUTTONS or main_button
        self.assertEqual(controls_allowed, self.safety.get_controls_allowed())

  def test_sampling_cruise_buttons(self):
    """
      Test that we allow controls on recent button press, but not as button leaves sliding window
    """
    self._rx(self._button_msg(Buttons.SET))
    for i in range(2 * PREV_BUTTON_SAMPLES):
      self._rx(self._pcm_status_msg(False))
      self.assertFalse(self.safety.get_controls_allowed())
      self._rx(self._pcm_status_msg(True))
      controls_allowed = i < PREV_BUTTON_SAMPLES
      self.assertEqual(controls_allowed, self.safety.get_controls_allowed())
      self._rx(self._button_msg(Buttons.NONE))