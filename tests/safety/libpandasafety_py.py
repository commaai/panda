import os
import subprocess

from cffi import FFI

can_dir = os.path.dirname(os.path.abspath(__file__))
libpandasafety_fn = os.path.join(can_dir, "libpandasafety.so")
subprocess.check_call(["make"], cwd=can_dir)

ffi = FFI()
ffi.cdef("""
typedef struct
{
  uint32_t TIR;  /*!< CAN TX mailbox identifier register */
  uint32_t TDTR; /*!< CAN mailbox data length control and time stamp register */
  uint32_t TDLR; /*!< CAN mailbox data low register */
  uint32_t TDHR; /*!< CAN mailbox data high register */
} CAN_TxMailBox_TypeDef;

typedef struct
{
  uint32_t RIR;  /*!< CAN receive FIFO mailbox identifier register */
  uint32_t RDTR; /*!< CAN receive FIFO mailbox data length control and time stamp register */
  uint32_t RDLR; /*!< CAN receive FIFO mailbox data low register */
  uint32_t RDHR; /*!< CAN receive FIFO mailbox data high register */
} CAN_FIFOMailBox_TypeDef;

typedef struct
{
  uint32_t CNT;
} TIM_TypeDef;

void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int toyota_tx_hook(CAN_FIFOMailBox_TypeDef *to_send);
void toyota_init(int16_t param);
void set_controls_allowed(int c);
int get_controls_allowed(void);

""")

libpandasafety = ffi.dlopen(libpandasafety_fn)
