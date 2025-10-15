#define BODY_CAN_DECI_RPM_SCALE   0.1f
#define BODY_CAN_MOTOR_MIN        1U
#define BODY_CAN_MOTOR_MAX        2U

#define BODY_CAN_ADDR_TARGET_RPM  0x600U

static volatile bool body_can_pending = false;
static volatile uint8_t body_can_motor = 0U;
static volatile int32_t body_can_target_deci_rpm = 0;

static bool body_can_motor_valid(uint8_t motor) {
  return (motor >= BODY_CAN_MOTOR_MIN) && (motor <= BODY_CAN_MOTOR_MAX);
}

void body_can_safety_rx(const CANPacket_t *msg) {
  if ((msg == NULL) || (msg->extended != 0U) || (msg->bus != 0U)) {
    return;
  }

  if ((msg->addr == BODY_CAN_ADDR_TARGET_RPM) && (dlc_to_len[msg->data_len_code] >= 3U)) {
    uint8_t motor = msg->data[0];
    if (body_can_motor_valid(motor)) {
      int16_t target_deci_rpm = (int16_t)((msg->data[2] << 8U) | msg->data[1]);
      ENTER_CRITICAL();
      body_can_motor = motor;
      body_can_target_deci_rpm = (int32_t)target_deci_rpm;
      body_can_pending = true;
      EXIT_CRITICAL();
    }
  }
}

void body_can_init(void) {
  ENTER_CRITICAL();
  body_can_pending = false;
  body_can_motor = 0U;
  body_can_target_deci_rpm = 0;
  EXIT_CRITICAL();

  can_silent = false;
  can_loopback = false;
  (void)set_safety_hooks(SAFETY_BODY, 0U);
  current_board->set_can_mode(CAN_MODE_NORMAL);
  current_board->enable_can_transceiver(1U, true);
  can_init_all();

  const uint32_t fdcan_irq_priority = 12U;
  NVIC_SetPriority(FDCAN1_IT0_IRQn, fdcan_irq_priority);
  NVIC_SetPriority(FDCAN1_IT1_IRQn, fdcan_irq_priority);
  NVIC_SetPriority(FDCAN2_IT0_IRQn, fdcan_irq_priority);
  NVIC_SetPriority(FDCAN2_IT1_IRQn, fdcan_irq_priority);
  NVIC_SetPriority(FDCAN3_IT0_IRQn, fdcan_irq_priority);
  NVIC_SetPriority(FDCAN3_IT1_IRQn, fdcan_irq_priority);
}

void body_can_periodic(uint32_t now) {
  (void)now;

  uint8_t motor;
  int32_t target_deci_rpm;

  ENTER_CRITICAL();
  if (body_can_pending) {
    body_can_pending = false;
    motor = body_can_motor;
    target_deci_rpm = body_can_target_deci_rpm;
  } else {
    motor = 0U;
    target_deci_rpm = 0;
  }
  EXIT_CRITICAL();

  if (motor == 0U) {
    return;
  }

  float target_rpm = ((float)target_deci_rpm) * BODY_CAN_DECI_RPM_SCALE;
  motor_speed_controller_set_target_rpm(motor, target_rpm);
}
