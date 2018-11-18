#!/usr/bin/env python
import time
import struct
from enum import Enum
import threading

# class Services(Enum):
#   DiagnosticSessionControl        = 0x10
#   ECUReset                        = 0x11
#   SecurityAccess                  = 0x27
#   CommunicationControl            = 0x28
#   TesterPresent                   = 0x3E
#   AccessTimingParameter           = 0x83
#   SecuredDataTransmission         = 0x84
#   ControlDTCSetting               = 0x85
#   ResponseOnEvent                 = 0x86
#   LinkControl                     = 0x87
#   ReadDataByIdentifier            = 0x22
#   ReadMemoryByAddress             = 0x23
#   ReadScalingDataByIdentifier     = 0x24
#   ReadDataByPeriodicIdentifier    = 0x2A
#   DynamicallyDefineDataIdentifier = 0x2C
#   WriteDataByIdentifier           = 0x2E
#   WriteMemoryByAddress            = 0x3D
#   ClearDiagnosticInformation      = 0x14
#   ReadDTCInformation              = 0x19
#   InputOutputControlByIdentifier  = 0x2F
#   RoutineControl                  = 0x31
#   RequestDownload                 = 0x34
#   RequestUpload                   = 0x35
#   TransferData                    = 0x36
#   RequestTransferExit             = 0x37

_negative_response_codes = {
    '\x00': 'positive response',
    '\x10': 'general reject',
    '\x11': 'service not supported',
    '\x12': 'sub-function not supported',
    '\x13': 'incorrect message length or invalid format',
    '\x14': 'response too long',
    '\x21': 'busy repeat request',
    '\x22': 'conditions not correct',
    '\x24': 'request sequence error',
    '\x25': 'no response from subnet component',
    '\x26': 'failure prevents execution of requested action',
    '\x31': 'request out of range',
    '\x33': 'security access denied',
    '\x35': 'invalid key',
    '\x36': 'exceed numebr of attempts',
    '\x37': 'required time delay not expired',
    '\x70': 'upload download not accepted',
    '\x71': 'transfer data suspended',
    '\x72': 'general programming failure',
    '\x73': 'wrong block sequence counter',
    '\x78': 'request correctly received - response pending',
    '\x7e': 'sub-function not supported in active session',
    '\x7f': 'service not supported in active session',
    '\x81': 'rpm too high',
    '\x82': 'rpm too low',
    '\x83': 'engine is running',
    '\x84': 'engine is not running',
    '\x85': 'engine run time too low',
    '\x86': 'temperature too high',
    '\x87': 'temperature too low',
    '\x88': 'vehicle speed too high',
    '\x89': 'vehicle speed too low',
    '\x8a': 'throttle/pedal too high',
    '\x8b': 'throttle/pedal too low',
    '\x8c': 'transmission not in neutral',
    '\x8d': 'transmission not in gear',
    '\x8f': 'brake switch(es) not closed',
    '\x90': 'shifter lever not in park',
    '\x91': 'torque converter clutch locked',
    '\x92': 'voltage too high',
    '\x93': 'voltage too low',
}

# generic uds request
def _request(address, service, subfunction, data=None):
  # TODO: send request
  # TODO: wait for response

  # raise exception on error
  if resp[0] == '\x7f'
    error_code = resp[2]
    error_desc = _negative_response_codes[error_code]
    raise Exception('[{}] {}'.format(hex(ord(error_code)), error_desc))

  resp_sid = ord(resp[0]) if len(resp) > 0 else None
  if service != resp_sid + 0x40:
    resp_sid_hex = hex(resp_sid) if resp_sid is not None else None
    raise Exception('invalid response service id: {}'.format(resp_sid_hex))

  if subfunction is None:
    resp_subf = ord(resp[1]) if len(resp) > 1 else None
    if subfunction != resp_subf:
      resp_subf_hex = hex(resp_subf) if resp_subf is not None else None
      raise Exception('invalid response subfunction: {}'.format(hex(resp_subf)))

  # return data (exclude service id and sub-function id)
  return resp[(1 if subfunction is None else 2):]

# services
class DIAGNOSTIC_SESSION_CONTROL_TYPE(Enum):
  DEFAULT = 1
  PROGRAMMING = 2
  EXTENDED_DIAGNOSTIC = 3
  SAFETY_SYSTEM_DIAGNOSTIC = 4

def diagnostic_session_control(address, session_type):
  _request(address, service=0x10, subfunction=session_type)

class ECU_RESET_TYPE(Enum):
  HARD = 1
  KEY_OFF_ON = 2
  SOFT = 3
  ENABLE_RAPID_POWER_SHUTDOWN = 4
  DISABLE_RAPID_POWER_SHUTDOWN = 5

def ecu_reset(address, reset_type):
  resp = _request(address, service=0x11, subfunction=reset_type)
  power_down_time = None
  if reset_type == RESET_TYPE.ENABLE_RAPID_POWER_SHUTDOWN
    power_down_time = ord(resp[0])
  return {"power_down_time": power_down_time}

class SECURITY_ACCESS_TYPE(Enum):
  REQUEST_SEED = 1
  SEND_KEY = 2

def security_access(address, access_type, security_key=None):
  request_seed = access_type % 2 == 0
  if request_seed and security_key is not None:
    raise ValueError('security_key not allowed')
  if not request_seed and security_key is None:
    raise ValueError('security_key is missing')
  resp = _request(address, service=0x27, subfunction=access_type, data=security_key)
  if request_seed:
    return {"security_seed": resp}

class COMMUNICATION_CONTROL_TYPE(Enum):
  ENABLE_RX_ENABLE_TX = 0
  ENABLE_RX_DISABLE_TX = 1
  DISABLE_RX_ENABLE_TX = 2
  DISABLE_RX_DISABLE_TX = 3

class COMMUNICATION_CONTROL_MESSAGE_TYPE(Enum):
  NORMAL = 1
  NETWORK_MANAGEMENT = 2
  NORMAL_AND_NETWORK_MANAGEMENT = 3

def communication_control(address, control_type, message_type):
  data = chr(message_type)
  _request(address, service=0x28, subfunction=control_type, data=data)

def tester_present(address):
  _request(address, service=0x3E, subfunction=0x00)

class ACCESS_TIMING_PARAMETER_TYPE(Enum):
  READ_EXTENDED_SET = 1
  SET_TO_DEFAULT_VALUES = 2
  READ_CURRENTLY_ACTIVE = 3
  SET_TO_GIVEN_VALUES = 4

def access_timing_parameter(address, parameter_type, parameter_values):
  write_custom_values = parameter_type == ACCESS_TIMING_PARAMETER_TYPE.SET_TO_GIVEN_VALUES
  read_values = (
    parameter_type == ACCESS_TIMING_PARAMETER_TYPE.READ_CURRENTLY_ACTIVE or
    parameter_type == ACCESS_TIMING_PARAMETER_TYPE.READ_EXTENDED_SET
  )
  if not write_custom_values and parameter_values is not None:
    raise ValueError('parameter_values not allowed')
  if write_custom_values and parameter_values is None:
    raise ValueError('parameter_values is missing')
  resp = _request(address, service=0x83, subfunction=parameter_type, data=parameter_values)
  if read_values:
    # TODO: parse response into values?
    return {"parameter_values": resp}

def secured_data_transmission(address, data):
  # TODO: split data into multiple input parameters?
  resp = _request(address, service=0x84, subfunction=None, data=data)
  # TODO: parse response into multiple output values?
  return {"data"=resp}

class CONTROL_DTC_SETTING_TYPE(Enum):
  ON = 1
  OFF = 2

def control_dtc_setting(address, setting_type):
  _request(address, service=0x85, subfunction=setting_type)

class RESPONSE_ON_EVENT_TYPE(Enum):
  STOP_RESPONSE_ON_EVENT = 0
  ON_DTC_STATUS_CHANGE = 1
  ON_TIMER_INTERRUPT = 2
  ON_CHANGE_OF_DATA_IDENTIFIER = 3
  REPORT_ACTIVATED_EVENTS = 4
  START_RESPONSE_ON_EVENT = 5
  CLEAR_RESPONSE_ON_EVENT = 6
  ON_COMPARISON_OF_VALUES = 7

def response_on_event(address, event_type, store_event, window_time, event_type_record, service_response_record):
  if store_event:
    event_type |= 0x20
  # TODO: split record parameters into arrays
  data = char(window_time) + event_type_record + service_response_record
  resp = _request(address, service=0x86, subfunction=event_type, data=data)
  # TODO: parse the reset of response

class LINK_CONTROL_TYPE(Enum):
  VERIFY_BAUDRATE_TRANSITION_WITH_FIXED_BAUDRATE = 1
  VERIFY_BAUDRATE_TRANSITION_WITH_SPECIFIC_BAUDRATE = 2
  TRANSITION_BAUDRATE = 3

class LINK_CONTROL_BAUD_RATE(Enum):
  PC9600 = 1
  PC19200 = 2
  PC38400 = 3
  PC57600 = 4
  PC115200 = 5
  CAN125000 = 16
  CAN250000 = 17
  CAN500000 = 18
  CAN1000000 = 19

def link_control(address, control_type, baud_rate=None):
  if LINK_CONTROL_TYPE.VERIFY_BAUDRATE_TRANSITION_WITH_FIXED_BAUDRATE:
    # baud_rate = LINK_CONTROL_BAUD_RATE
    data = chr(baud_rate)
  elif LINK_CONTROL_TYPE.VERIFY_BAUDRATE_TRANSITION_WITH_SPECIFIC_BAUDRATE:
    # baud_rate = custom value (3 bytes big-endian)
    data = struct.pack('!I', baud_rate)[1:]
  else:
    data = None
  _request(address, service=0x87, subfunction=control_type, data=data)

class DATA_IDENTIFIER(Enum):
  BOOT_SOFTWARE_IDENTIFICATION = 0XF180
  APPLICATION_SOFTWARE_IDENTIFICATION = 0XF181
  APPLICATION_DATA_IDENTIFICATION = 0XF182
  BOOT_SOFTWARE_FINGERPRINT = 0XF183
  APPLICATION_SOFTWARE_FINGERPRINT = 0XF184
  APPLICATION_DATA_FINGERPRINT = 0XF185
  ACTIVE_DIAGNOSTIC_SESSION = 0XF186
  VEHICLE_MANUFACTURER_SPARE_PART_NUMBER = 0XF187
  VEHICLE_MANUFACTURER_ECU_SOFTWARE_NUMBER = 0XF188
  VEHICLE_MANUFACTURER_ECU_SOFTWARE_VERSION_NUMBER = 0XF189
  SYSTEM_SUPPLIER_IDENTIFIER = 0XF18A
  ECU_MANUFACTURING_DATE = 0XF18B
  ECU_SERIAL_NUMBER = 0XF18C
  SUPPORTED_FUNCTIONAL_UNITS = 0XF18D
  VEHICLE_MANUFACTURER_KIT_ASSEMBLY_PART_NUMBER = 0XF18E
  VIN = 0XF190
  VEHICLE_MANUFACTURER_ECU_HARDWARE_NUMBER = 0XF191
  SYSTEM_SUPPLIER_ECU_HARDWARE_NUMBER = 0XF192
  SYSTEM_SUPPLIER_ECU_HARDWARE_VERSION_NUMBER = 0XF193
  SYSTEM_SUPPLIER_ECU_SOFTWARE_NUMBER = 0XF194
  SYSTEM_SUPPLIER_ECU_SOFTWARE_VERSION_NUMBER = 0XF195
  EXHAUST_REGULATION_OR_TYPE_APPROVAL_NUMBER = 0XF196
  SYSTEM_NAME_OR_ENGINE_TYPE = 0XF197
  REPAIR_SHOP_CODE_OR_TESTER_SERIAL_NUMBER = 0XF198
  PROGRAMMING_DATE = 0XF199
  CALIBRATION_REPAIR_SHOP_CODE_OR_CALIBRATION_EQUIPMENT_SERIAL_NUMBER = 0XF19A
  CALIBRATION_DATE = 0XF19B
  CALIBRATION_EQUIPMENT_SOFTWARE_NUMBER = 0XF19C
  ECU_INSTALLATION_DATE = 0XF19D
  ODX_FILE = 0XF19E
  ENTITY = 0XF19F

def read_data_by_identifier(address, data_identifier):
  # TODO: support list of identifiers
  data = struct.pack('!H', data_id)
  resp = _request(address, service=0x22, subfunction=None, data=data)
  resp_id = struct.unpack('!H', data[0:2])[0] if len(data) >= 2 else None
  if resp_id != data_id:
    raise ValueError('invalid response data identifier: {}'.format(hex(resp_id)))
  return resp[2:]

def read_memory_by_address(address, memory_address, memory_size, memory_address_bytes=4, memory_size_bytes=4):
  if memory_address_bytes < 1 or memory_address_bytes > 4:
    raise ValueError('invalid memory_address_bytes: {}'.format(memory_address_bytes))
  if memory_size_bytes < 1 or memory_size_bytes > 4:
    raise ValueError('invalid memory_size_bytes: {}'.format(memory_size_bytes))
  data = struct.pack('!BB', memory_size_bytes, memory_address_bytes)

  if memory_address >= 1<<(memory_address_bytes*8)
    raise ValueError('invalid memory_address: {}'.format(memory_address))
  data += struct.pack('!I', memory_address)[4-memory_address_bytes:]
  if memory_size >= 1<<(memory_size_bytes*8)
    raise ValueError('invalid memory_size: {}'.format(memory_address))
  data += struct.pack('!I', memory_size)[4-memory_size_bytes:]

  resp = _request(address, service=0x23, subfunction=None, data=data)
  return resp

def read_scaling_data_by_identifier(address):
  raise NotImplementedError()
  _request(address, service=0x24, subfunction=0x00)

def read_data_by_periodic_identifier(address):
  raise NotImplementedError()
  _request(address, service=0x2A, subfunction=0x00)

def dynamically_define_data_identifier(address):
  raise NotImplementedError()
  _request(address, service=0x2C, subfunction=0x00)

def write_data_by_identifier(address):
  raise NotImplementedError()
  _request(address, service=0x2E, subfunction=0x00)

def write_memory_by_address(address):
  raise NotImplementedError()
  _request(address, service=0x3D, subfunction=0x00)

def clear_diagnostic_information(address):
  raise NotImplementedError()
  _request(address, service=0x14, subfunction=0x00)

def read_dtc_information(address):
  raise NotImplementedError()
  _request(address, service=0x19, subfunction=0x00)

class INPUT_OUTPUT_CONTROL_PARAMETER(Enum):
  RETURN_CONTROL_TO_ECU = 0
  RESET_TO_DEFAULT = 1
  FREEZE_CURRENT_STATE = 2
  SHORT_TERM_ADJUSTMENT = 3

def input_output_control_by_identifier(address):
  raise NotImplementedError()
  _request(address, service=0x2F, subfunction=0x00)

class ROUTINE_CONTROL_TYPE(Enum):
  ERASE_MEMORY = 0xFF00
  CHECK_PROGRAMMING_DEPENDENCIES = 0xFF01
  ERASE_MIRROR_MEMORY_DTCS = 0xFF02

def routine_control(address):
  raise NotImplementedError()
  _request(address, service=0x31, subfunction=0x00)

def request_download(address):
  raise NotImplementedError()
  _request(address, service=0x34, subfunction=0x00)

def request_upload(address):
  raise NotImplementedError()
  _request(address, service=0x35, subfunction=0x00)

def transfer_data(address):
  raise NotImplementedError()
  _request(address, service=0x36, subfunction=0x00)
  
def request_transfer_exit(address)
  raise NotImplementedError()
  _request(address, service=0x37, subfunction=0x00)

if __name__ == "__main__":
  from . import uds
  # examples
  vin = uds.read_data_by_identifier(0x18da10f1, uds.DATA_IDENTIFIER.VIN)
