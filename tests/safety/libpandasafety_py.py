import os

from typing import List
from cffi import FFI

can_dir = os.path.dirname(os.path.abspath(__file__))
libpandasafety_fn = os.path.join(can_dir, "libpandasafety.so")

ffi = FFI()
ffi.cdef("""
typedef struct {
  unsigned char reserved : 1;
  unsigned char bus : 3;
  unsigned char data_len_code : 4;
  unsigned char rejected : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;
  unsigned int addr : 29;
  unsigned char data[64];
} CANPacket_t;
""", packed=True)

ffi.cdef("""
typedef struct
{
  uint32_t CNT;
} TIM_TypeDef;

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

bool get_cruise_engaged_prev(void);
bool get_vehicle_moving(void);
int get_hw_type(void);
void set_timer(uint32_t t);

int safety_rx_hook(CANPacket_t *to_send);
int safety_tx_hook(CANPacket_t *to_push);
int safety_fwd_hook(int bus_num, CANPacket_t *to_fwd);
int set_safety_hooks(uint16_t mode, uint16_t param);

void safety_tick_current_rx_checks();
bool addr_checks_valid();

void init_tests(void);

void init_tests_honda(void);
void set_honda_fwd_brake(bool c);
void set_honda_alt_brake_msg(bool c);
void set_honda_bosch_long(bool c);
int get_honda_hw(void);

""")


class CANPacket:
  reserved: int
  bus: int
  data_len_code: int
  rejected: int
  returned: int
  extended: int
  addr: int
  data: List[int]


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

  def get_cruise_engaged_prev(self) -> bool: ...
  def get_vehicle_moving(self) -> bool: ...
  def get_hw_type(self) -> int: ...
  def set_timer(self, t: int) -> None: ...

  def safety_rx_hook(self, to_send: CANPacket) -> int: ...
  def safety_tx_hook(self, to_push: CANPacket) -> int: ...
  def safety_fwd_hook(self, bus_num: int, to_fwd: CANPacket) -> int: ...
  def set_safety_hooks(self, mode: int, param: int) -> int: ...

  def safety_tick_current_rx_checks(self) -> None: ...
  def addr_checks_valid(self) -> bool: ...

  def init_tests(self) -> None: ...

  def init_tests_honda(self) -> None: ...
  def set_honda_fwd_brake(self, c: bool) -> None: ...
  def set_honda_alt_brake_msg(self, c: bool) -> None: ...
  def set_honda_bosch_long(self, c: bool) -> None: ...
  def get_honda_hw(self) -> int: ...

libpandasafety: PandaSafety = ffi.dlopen(libpandasafety_fn)
