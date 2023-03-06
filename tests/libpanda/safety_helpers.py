# panda safety helpers, from safety_helpers.c

def setup_safety_helpers(ffi):
  ffi.cdef("""
  void set_controls_allowed(bool c);
  bool get_controls_allowed(void);
  bool get_longitudinal_allowed(void);
  void set_alternative_experience(int mode);
  int get_alternative_experience(void);
  void set_relay_malfunction(bool c);
  bool get_relay_malfunction(void);
  void set_gas_interceptor_detected(bool c);
  bool get_gas_interceptor_detected(void);
  int get_gas_interceptor_prev(void);
  bool get_gas_pressed_prev(void);
  bool get_brake_pressed_prev(void);
  bool get_regen_braking_prev(void);
  bool get_acc_main_on(void);

  void set_torque_meas(int min, int max);
  int get_torque_meas_min(void);
  int get_torque_meas_max(void);
  void set_torque_driver(int min, int max);
  int get_torque_driver_min(void);
  int get_torque_driver_max(void);
  void set_desired_torque_last(int t);
  void set_rt_torque_last(int t);
  void set_desired_angle_last(int t);
  float get_debug_value(void);
  float get_debug_value_2(void);
  float get_debug_value_3(void);

  bool get_cruise_engaged_prev(void);
  bool get_vehicle_moving(void);
  float get_vehicle_speed(void);
  int get_hw_type(void);
  void set_timer(uint32_t t);

  void safety_tick_current_rx_checks();
  bool addr_checks_valid();

  void init_tests(void);

  void set_honda_fwd_brake(bool c);
  void set_honda_alt_brake_msg(bool c);
  void set_honda_bosch_long(bool c);
  int get_honda_hw(void);
  """)

class PandaSafety:
  def set_controls_allowed(self, c: bool) -> None: ...
  def get_controls_allowed(self) -> bool: ...
  def get_longitudinal_allowed(self) -> bool: ...
  def set_alternative_experience(self, mode: int) -> None: ...
  def get_alternative_experience(self) -> int: ...
  def set_relay_malfunction(self, c: bool) -> None: ...
  def get_relay_malfunction(self) -> bool: ...
  def set_gas_interceptor_detected(self, c: bool) -> None: ...
  def get_gas_interceptor_detected(self) -> bool: ...
  def get_gas_interceptor_prev(self) -> int: ...
  def get_gas_pressed_prev(self) -> bool: ...
  def get_brake_pressed_prev(self) -> bool: ...
  def get_regen_braking_prev(self) -> bool: ...
  def get_acc_main_on(self) -> bool: ...

  def set_torque_meas(self, min: int, max: int) -> None: ...  # pylint: disable=redefined-builtin
  def get_torque_meas_min(self) -> int: ...
  def get_torque_meas_max(self) -> int: ...
  def set_torque_driver(self, min: int, max: int) -> None: ...  # pylint: disable=redefined-builtin
  def get_torque_driver_min(self) -> int: ...
  def get_torque_driver_max(self) -> int: ...
  def set_desired_torque_last(self, t: int) -> None: ...
  def set_rt_torque_last(self, t: int) -> None: ...
  def set_desired_angle_last(self, t: int) -> None: ...
  def get_debug_value(self) -> float: ...
  def get_debug_value_2(self) -> float: ...
  def get_debug_value_3(self) -> float: ...

  def get_cruise_engaged_prev(self) -> bool: ...
  def get_vehicle_moving(self) -> bool: ...
  def get_vehicle_speed(self) -> float: ...
  def get_hw_type(self) -> int: ...
  def set_timer(self, t: int) -> None: ...

  def safety_tick_current_rx_checks(self) -> None: ...
  def addr_checks_valid(self) -> bool: ...

  def init_tests(self) -> None: ...

  def set_honda_fwd_brake(self, c: bool) -> None: ...
  def set_honda_alt_brake_msg(self, c: bool) -> None: ...
  def set_honda_bosch_long(self, c: bool) -> None: ...
  def get_honda_hw(self) -> int: ...


