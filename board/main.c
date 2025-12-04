// ********************* Includes *********************
#include "board/config.h"

#include "board/drivers/led.h"
#include "board/drivers/pwm.h"
#include "board/drivers/usb.h"
#include "board/drivers/simple_watchdog.h"
#include "board/drivers/bootkick.h"

#include "board/early_init.h"
#include "board/provision.h"

#include "libc.h"

#include "opendbc/safety/safety.h"

#include "board/health.h"

#include "board/drivers/can_common.h"

#include "board/drivers/fdcan.h"

#include "board/power_saving.h"

#include "board/obj/gitversion.h"

#include "board/can_comms.h"
#include "board/main_comms.h"
#include "board/main_declarations.h"


// TODO
uint32_t heartbeat_counter;
bool heartbeat_lost;
bool heartbeat_disabled;
bool siren_enabled;
uint32_t uptime_cnt;

// ********************* Serial debugging *********************

void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (get_char(ring, &rcv)) {
    (void)put_char(ring, rcv);  // misra-c2012-17.7: cast to void is ok: debug function
  }
}

// ****************************** safety mode ******************************

#include <stdint.h>
#include <stdbool.h>

#include "board/utils.h"
#include "board/config.h"
#include "board/health.h"
#include "board/main_declarations.h"
#include "board/drivers/can_common.h"
#include "board/power_saving.h"
#include "board/drivers/spi.h"
#include "board/drivers/bootkick.h"
#include "board/drivers/fdcan.h"
#include "board/provision.h"
#include "board/early_init.h"
#include "board/drivers/fan.h"
#include "board/drivers/clock_source.h"

extern int _app_start[0xc000]; // Only first 3 sectors of size 0x4000 are used

// Prototypes
void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);

static int get_health_pkt(void *dat) {
  COMPILE_TIME_ASSERT(sizeof(struct health_t) <= USBPACKET_MAX_SIZE);
  struct health_t * health = (struct health_t*)dat;

  health->uptime_pkt = uptime_cnt;
  health->voltage_pkt = current_board->read_voltage_mV();
  health->current_pkt = current_board->read_current_mA();

  health->ignition_line_pkt = (uint8_t)(harness_check_ignition());
  health->ignition_can_pkt = ignition_can;

  health->controls_allowed_pkt = controls_allowed;
  health->safety_tx_blocked_pkt = safety_tx_blocked;
  health->safety_rx_invalid_pkt = safety_rx_invalid;
  health->tx_buffer_overflow_pkt = tx_buffer_overflow;
  health->rx_buffer_overflow_pkt = rx_buffer_overflow;
  health->car_harness_status_pkt = harness.status;
  health->safety_mode_pkt = (uint8_t)(current_safety_mode);
  health->safety_param_pkt = current_safety_param;
  health->alternative_experience_pkt = alternative_experience;
  health->power_save_enabled_pkt = power_save_status == POWER_SAVE_STATUS_ENABLED;
  health->heartbeat_lost_pkt = heartbeat_lost;
  health->safety_rx_checks_invalid_pkt = safety_rx_checks_invalid;

  health->spi_error_count_pkt = spi_error_count;

  health->fault_status_pkt = fault_status;
  health->faults_pkt = faults;

  health->interrupt_load_pkt = interrupt_load;

  health->fan_power = fan_state.power;

  health->sbu1_voltage_mV = harness.sbu1_voltage_mV;
  health->sbu2_voltage_mV = harness.sbu2_voltage_mV;

  health->som_reset_triggered = bootkick_reset_triggered;

  return sizeof(*health);
}

// send on serial, first byte to select the ring
void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  uart_ring *ur = get_ring_by_number(data[0]);
  if ((len != 0U) && (ur != NULL)) {
    if ((data[0] < 2U) || (data[0] >= 4U)) {
      for (uint32_t i = 1; i < len; i++) {
        while (!put_char(ur, data[i])) {
          // wait
        }
      }
    }
  }
}

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  unsigned int resp_len = 0;
  uart_ring *ur = NULL;
  uint32_t time;

#ifdef DEBUG_COMMS
  print("raw control request: "); hexdump(req, sizeof(ControlPacket_t)); print("\n");
  print("- request "); puth(req->request); print("\n");
  print("- param1 "); puth(req->param1); print("\n");
  print("- param2 "); puth(req->param2); print("\n");
#endif

  switch (req->request) {
    // **** 0xa8: get microsecond timer
    case 0xa8:
      time = microsecond_timer_get();
      resp[0] = (time & 0x000000FFU);
      resp[1] = ((time & 0x0000FF00U) >> 8U);
      resp[2] = ((time & 0x00FF0000U) >> 16U);
      resp[3] = ((time & 0xFF000000U) >> 24U);
      resp_len = 4U;
      break;
    // **** 0xb0: set IR power
    case 0xb0:
      current_board->set_ir_power(req->param1);
      break;
    // **** 0xb1: set fan power
    case 0xb1:
      fan_set_power(req->param1);
      break;
    // **** 0xb2: get fan rpm
    case 0xb2:
      resp[0] = (fan_state.rpm & 0x00FFU);
      resp[1] = ((fan_state.rpm & 0xFF00U) >> 8U);
      resp_len = 2;
      break;
    // **** 0xc0: reset communications state
    case 0xc0:
      comms_can_reset();
      break;
    // **** 0xc1: get hardware type
    case 0xc1:
      resp[0] = hw_type;
      resp_len = 1;
      break;
    // **** 0xc2: CAN health stats
    case 0xc2:
      COMPILE_TIME_ASSERT(sizeof(can_health_t) <= USBPACKET_MAX_SIZE);
      if (req->param1 < 3U) {
        update_can_health_pkt(req->param1, 0U);
        can_health[req->param1].can_speed = (bus_config[req->param1].can_speed / 10U);
        can_health[req->param1].can_data_speed = (bus_config[req->param1].can_data_speed / 10U);
        can_health[req->param1].canfd_enabled = bus_config[req->param1].canfd_enabled;
        can_health[req->param1].brs_enabled = bus_config[req->param1].brs_enabled;
        can_health[req->param1].canfd_non_iso = bus_config[req->param1].canfd_non_iso;
        resp_len = sizeof(can_health[req->param1]);
        (void)memcpy(resp, (uint8_t*)(&can_health[req->param1]), resp_len);
      }
      break;
    // **** 0xc3: fetch MCU UID
    case 0xc3:
      (void)memcpy(resp, ((uint8_t *)UID_BASE), 12);
      resp_len = 12;
      break;
    // **** 0xc4: get interrupt call rate
    case 0xc4:
      if (req->param1 < NUM_INTERRUPTS) {
        uint32_t load = interrupts[req->param1].call_rate;
        resp[0] = (load & 0x000000FFU);
        resp[1] = ((load & 0x0000FF00U) >> 8U);
        resp[2] = ((load & 0x00FF0000U) >> 16U);
        resp[3] = ((load & 0xFF000000U) >> 24U);
        resp_len = 4U;
      }
      break;
    // **** 0xc5: DEBUG: drive relay
    case 0xc5:
      set_intercept_relay((req->param1 & 0x1U), (req->param1 & 0x2U));
      break;
    // **** 0xc6: DEBUG: read SOM GPIO
    case 0xc6:
      resp[0] = current_board->read_som_gpio();
      resp_len = 1;
      break;
    // **** 0xd0: fetch serial (aka the provisioned dongle ID)
    case 0xd0:
      // addresses are OTP
      if (req->param1 == 1U) {
        (void)memcpy(resp, (uint8_t *)DEVICE_SERIAL_NUMBER_ADDRESS, 0x10);
        resp_len = 0x10;
      } else {
        get_provision_chunk(resp);
        resp_len = PROVISION_CHUNK_LEN;
      }
      break;
    // **** 0xd1: enter bootloader mode
    case 0xd1:
      // this allows reflashing of the bootstub
      switch (req->param1) {
        case 0:
          // only allow bootloader entry on debug builds
          #ifdef ALLOW_DEBUG
            print("-> entering bootloader\n");
            enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
            NVIC_SystemReset();
          #endif
          break;
        case 1:
          print("-> entering softloader\n");
          enter_bootloader_mode = ENTER_SOFTLOADER_MAGIC;
          NVIC_SystemReset();
          break;
        default:
          print("Bootloader mode invalid\n");
          break;
      }
      break;
    // **** 0xd2: get health packet
    case 0xd2:
      resp_len = get_health_pkt(resp);
      break;
    // **** 0xd3: get first 64 bytes of signature
    case 0xd3:
      {
        resp_len = 64;
        char * code = (char*)_app_start;
        int code_len = _app_start[0];
        (void)memcpy(resp, &code[code_len], resp_len);
      }
      break;
    // **** 0xd4: get second 64 bytes of signature
    case 0xd4:
      {
        resp_len = 64;
        char * code = (char*)_app_start;
        int code_len = _app_start[0];
        (void)memcpy(resp, &code[code_len + 64], resp_len);
      }
      break;
    // **** 0xd6: get version
    case 0xd6:
      COMPILE_TIME_ASSERT(sizeof(gitversion) <= USBPACKET_MAX_SIZE);
      (void)memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = sizeof(gitversion) - 1U;
      break;
    // **** 0xd8: reset ST
    case 0xd8:
      NVIC_SystemReset();
      break;
    // **** 0xdb: set OBD CAN multiplexing mode
    case 0xdb:
      current_board->set_can_mode((req->param1 == 1U) ? CAN_MODE_OBD_CAN2 : CAN_MODE_NORMAL);
      break;
    // **** 0xdc: set safety mode
    case 0xdc:
      set_safety_mode(req->param1, (uint16_t)req->param2);
      break;
    // **** 0xdd: get healthpacket and CANPacket versions
    case 0xdd:
      resp[0] = HEALTH_PACKET_VERSION;
      resp[1] = CAN_PACKET_VERSION;
      resp[2] = CAN_HEALTH_PACKET_VERSION;
      resp_len = 3;
      break;
    // **** 0xde: set can bitrate
    case 0xde:
      if ((req->param1 < PANDA_CAN_CNT) && is_speed_valid(req->param2, speeds, sizeof(speeds)/sizeof(speeds[0]))) {
        bus_config[req->param1].can_speed = req->param2;
        bool ret = can_init(CAN_NUM_FROM_BUS_NUM(req->param1));
        UNUSED(ret);
      }
      break;
    // **** 0xdf: set alternative experience
    case 0xdf:
      // you can only set this if you are in a non car safety mode
      if (!is_car_safety_mode(current_safety_mode)) {
        alternative_experience = req->param1;
      }
      break;
    // **** 0xe0: uart read
    case 0xe0:
      ur = get_ring_by_number(req->param1);
      if (!ur) {
        break;
      }

      // read
      uint16_t req_length = MIN(req->length, USBPACKET_MAX_SIZE);
      while ((resp_len < req_length) &&
                         get_char(ur, (char*)&resp[resp_len])) {
        ++resp_len;
      }
      break;
    // **** 0xe5: set CAN loopback (for testing)
    case 0xe5:
      can_loopback = req->param1 > 0U;
      can_init_all();
      break;
    // **** 0xe6: set custom clock source period and pulse length
    case 0xe6:
      clock_source_set_timer_params(req->param1, req->param2);
      break;
    // **** 0xe7: set power save state
    case 0xe7:
      set_power_save_state(req->param1);
      break;
    // **** 0xe8: set can-fd auto swithing mode
    case 0xe8:
      bus_config[req->param1].canfd_auto = req->param2 > 0U;
      break;
    // **** 0xf1: Clear CAN ring buffer.
    case 0xf1:
      if (req->param1 == 0xFFFFU) {
        print("Clearing CAN Rx queue\n");
        can_clear(&can_rx_q);
      } else if (req->param1 < PANDA_CAN_CNT) {
        print("Clearing CAN Tx queue\n");
        can_clear(can_queues[req->param1]);
      } else {
        print("Clearing CAN CAN ring buffer failed: wrong bus number\n");
      }
      break;
    // **** 0xf3: Heartbeat. Resets heartbeat counter.
    case 0xf3:
      {
        heartbeat_counter = 0U;
        heartbeat_lost = false;
        heartbeat_disabled = false;
        heartbeat_engaged = (req->param1 == 1U);
        break;
      }
    // **** 0xf6: set siren enabled
    case 0xf6:
      siren_enabled = (req->param1 != 0U);
      break;
    // **** 0xf8: disable heartbeat checks
    case 0xf8:
      if (!is_car_safety_mode(current_safety_mode)) {
        heartbeat_disabled = true;
      }
      break;
    // **** 0xf9: set CAN FD data bitrate
    case 0xf9:
      if ((req->param1 < PANDA_CAN_CNT) &&
           is_speed_valid(req->param2, data_speeds, sizeof(data_speeds)/sizeof(data_speeds[0]))) {
        bus_config[req->param1].can_data_speed = req->param2;
        bus_config[req->param1].canfd_enabled = (req->param2 >= bus_config[req->param1].can_speed);
        bus_config[req->param1].brs_enabled = (req->param2 > bus_config[req->param1].can_speed);
        bool ret = can_init(CAN_NUM_FROM_BUS_NUM(req->param1));
        UNUSED(ret);
      }
      break;
    // **** 0xfc: set CAN FD non-ISO mode
    case 0xfc:
      if (req->param1 < PANDA_CAN_CNT) {
        bus_config[req->param1].canfd_non_iso = (req->param2 != 0U);
        bool ret = can_init(CAN_NUM_FROM_BUS_NUM(req->param1));
        UNUSED(ret);
      }
      break;
    default:
      print("NO HANDLER ");
      puth(req->request);
      print("\n");
      break;
  }
  return resp_len;
}


// this is the only way to leave silent mode
void set_safety_mode(uint16_t mode, uint16_t param) {
  uint16_t mode_copy = mode;
  int err = set_safety_hooks(mode_copy, param);
  if (err == -1) {
    print("Error: safety set mode failed. Falling back to SILENT\n");
    mode_copy = SAFETY_SILENT;
    err = set_safety_hooks(mode_copy, 0U);
    // TERMINAL ERROR: we can't continue if SILENT safety mode isn't succesfully set
    assert_fatal(err == 0, "Error: Failed setting SILENT mode. Hanging\n");
  }
  safety_tx_blocked = 0;
  safety_rx_invalid = 0;

  switch (mode_copy) {
    case SAFETY_SILENT:
      set_intercept_relay(false, false);
      current_board->set_can_mode(CAN_MODE_NORMAL);
      can_silent = true;
      break;
    case SAFETY_NOOUTPUT:
      set_intercept_relay(false, false);
      current_board->set_can_mode(CAN_MODE_NORMAL);
      can_silent = false;
      break;
    case SAFETY_ELM327:
      set_intercept_relay(false, false);
      heartbeat_counter = 0U;
      heartbeat_lost = false;

      // Clear any pending messages in the can core (i.e. sending while comma power is unplugged)
      // TODO: rewrite using hardware queues rather than fifo to cancel specific messages
      can_clear_send(CANIF_FROM_CAN_NUM(1), 1);
      if (param == 0U) {
        current_board->set_can_mode(CAN_MODE_OBD_CAN2);
      } else {
        current_board->set_can_mode(CAN_MODE_NORMAL);
      }
      can_silent = false;
      break;
    default:
      set_intercept_relay(true, false);
      heartbeat_counter = 0U;
      heartbeat_lost = false;
      current_board->set_can_mode(CAN_MODE_NORMAL);
      can_silent = false;
      break;
  }
  can_init_all();
}

bool is_car_safety_mode(uint16_t mode) {
  return (mode != SAFETY_SILENT) &&
         (mode != SAFETY_NOOUTPUT) &&
         (mode != SAFETY_ALLOUTPUT) &&
         (mode != SAFETY_ELM327);
}

// ***************************** main code *****************************

// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
// cppcheck-suppress misra-c2012-8.4
void __initialize_hardware_early(void) {
  early_initialization();
}

static void __attribute__ ((noinline)) enable_fpu(void) {
  // enable the FPU
  SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
}

// go into SILENT when heartbeat isn't received for this amount of seconds.
#define HEARTBEAT_IGNITION_CNT_ON 5U
#define HEARTBEAT_IGNITION_CNT_OFF 2U

// called at 8Hz
static void tick_handler(void) {
  static uint32_t siren_countdown = 0; // siren plays while countdown > 0
  static uint32_t controls_allowed_countdown = 0;
  static uint8_t prev_harness_status = HARNESS_STATUS_NC;
  static uint8_t loop_counter = 0U;
  static bool relay_malfunction_prev = false;

  if (TICK_TIMER->SR != 0U) {

    // siren
    current_board->set_siren((loop_counter & 1U) && (siren_enabled || (siren_countdown > 0U)));

    // tick drivers at 8Hz
    fan_tick();
    harness_tick();
    simple_watchdog_kick();
    sound_tick();

    if (relay_malfunction_prev != relay_malfunction) {
      if (relay_malfunction) {
        fault_occurred(FAULT_RELAY_MALFUNCTION);
      } else {
        fault_recovered(FAULT_RELAY_MALFUNCTION);
      }
    }
    relay_malfunction_prev = relay_malfunction;

    // re-init everything that uses harness status
    if (harness.status != prev_harness_status) {
      prev_harness_status = harness.status;
      can_set_orientation(harness.status == HARNESS_STATUS_FLIPPED);

      // re-init everything that uses harness status
      can_init_all();
      set_safety_mode(current_safety_mode, current_safety_param);
      set_power_save_state(power_save_status);
    }

    // decimated to 1Hz
    if (loop_counter == 0U) {
      //puth(usart1_dma); print(" "); puth(DMA2_Stream5->M0AR); print(" "); puth(DMA2_Stream5->NDTR); print("\n");
      #ifdef DEBUG
        print("** blink ");
        print("rx:"); puth4(can_rx_q.r_ptr); print("-"); puth4(can_rx_q.w_ptr); print("  ");
        print("tx1:"); puth4(can_tx1_q.r_ptr); print("-"); puth4(can_tx1_q.w_ptr); print("  ");
        print("tx2:"); puth4(can_tx2_q.r_ptr); print("-"); puth4(can_tx2_q.w_ptr); print("  ");
        print("tx3:"); puth4(can_tx3_q.r_ptr); print("-"); puth4(can_tx3_q.w_ptr); print("\n");
      #endif

      // set green LED to be controls allowed
      led_set(LED_GREEN, controls_allowed);

      // turn off the blue LED, turned on by CAN
      // unless we are in power saving mode
      led_set(LED_BLUE, (uptime_cnt & 1U) && (power_save_status == POWER_SAVE_STATUS_ENABLED));

      const bool recent_heartbeat = heartbeat_counter == 0U;

      // tick drivers at 1Hz
      bool started = harness_check_ignition() || ignition_can;
      bootkick_tick(started, recent_heartbeat);

      // increase heartbeat counter and cap it at the uint32 limit
      if (heartbeat_counter < UINT32_MAX) {
        heartbeat_counter += 1U;
      }

      // disabling heartbeat not allowed while in safety mode
      if (is_car_safety_mode(current_safety_mode)) {
        heartbeat_disabled = false;
      }

      if (siren_countdown > 0U) {
        siren_countdown -= 1U;
      }

      if (controls_allowed || heartbeat_engaged) {
        controls_allowed_countdown = 5U;
      } else if (controls_allowed_countdown > 0U) {
        controls_allowed_countdown -= 1U;
      } else {

      }

      // exit controls allowed if unused by openpilot for a few seconds
      if (controls_allowed && !heartbeat_engaged) {
        heartbeat_engaged_mismatches += 1U;
        if (heartbeat_engaged_mismatches >= 3U) {
          controls_allowed = false;
        }
      } else {
        heartbeat_engaged_mismatches = 0U;
      }

      if (!heartbeat_disabled) {
        // if the heartbeat has been gone for a while, go to SILENT safety mode and enter power save
        if (heartbeat_counter >= (started ? HEARTBEAT_IGNITION_CNT_ON : HEARTBEAT_IGNITION_CNT_OFF)) {
          print("device hasn't sent a heartbeat for 0x");
          puth(heartbeat_counter);
          print(" seconds. Safety is set to SILENT mode.\n");

          if (controls_allowed_countdown > 0U) {
            siren_countdown = 3U;
            controls_allowed_countdown = 0U;
          }

          // set flag to indicate the heartbeat was lost
          if (is_car_safety_mode(current_safety_mode)) {
            heartbeat_lost = true;
          }

          // clear heartbeat engaged state
          heartbeat_engaged = false;

          if (current_safety_mode != SAFETY_SILENT) {
            set_safety_mode(SAFETY_SILENT, 0U);
          }

          if (power_save_status != POWER_SAVE_STATUS_ENABLED) {
            set_power_save_state(POWER_SAVE_STATUS_ENABLED);
          }

          // Also disable IR when the heartbeat goes missing
          current_board->set_ir_power(0U);

          // Run fan when device is up but not talking to us.
          // The bootloader enables the SOM GPIO on boot.
          fan_set_power(current_board->read_som_gpio() ? 30U : 0U);
        }
      }

      // check registers
      check_registers();

      // set ignition_can to false after 2s of no CAN seen
      if (ignition_can_cnt > 2U) {
        ignition_can = false;
      }

      // on to the next one
      uptime_cnt += 1U;
      safety_mode_cnt += 1U;
      ignition_can_cnt += 1U;

      // synchronous safety check
      safety_tick(&current_safety_config);
    }

    loop_counter++;
    loop_counter %= 8U;
  }
  TICK_TIMER->SR = 0;
}

int main(void) {
  // Init interrupt table
  init_interrupts(true);

  // shouldn't have interrupts here, but just in case
  disable_interrupts();

  // init early devices
  clock_init();
  peripherals_init();
  detect_board_type();
  led_init();
  // red+green leds enabled until succesful USB/SPI init, as a debug indicator
  led_set(LED_RED, true);
  led_set(LED_GREEN, true);
  adc_init(ADC1);

  // print hello
  print("\n\n\n************************ MAIN START ************************\n");

  // check for non-supported board types
  assert_fatal(hw_type != HW_TYPE_UNKNOWN, "Unsupported board type");

  print("Config:\n");
  print("  Board type: 0x"); puth(hw_type); print("\n");

  // init board
  current_board->init();
  current_board->set_can_mode(CAN_MODE_NORMAL);
  harness_init();

  // panda has an FPU, let's use it!
  enable_fpu();

  microsecond_timer_init();

  current_board->set_siren(false);
  if (current_board->has_fan) {
    fan_init();
  }

  // init to SILENT and can silent
  set_safety_mode(SAFETY_SILENT, 0U);

  // enable CAN TXs
  enable_can_transceivers(true);

  // init watchdog for heartbeat loop, fed at 8Hz
  simple_watchdog_init(FAULT_HEARTBEAT_LOOP_WATCHDOG, (3U * 1000000U / 8U));

  // 8Hz timer
  REGISTER_INTERRUPT(TICK_TIMER_IRQ, tick_handler, 10U, FAULT_INTERRUPT_RATE_TICK)
  tick_timer_init();

#ifdef DEBUG
  print("DEBUG ENABLED\n");
#endif
  // enable USB (right before interrupts or enum can fail!)
  usb_init();

  if (current_board->has_spi) {
    gpio_spi_init();
    spi_init();
  }

  led_set(LED_RED, false);
  led_set(LED_GREEN, false);
  led_set(LED_BLUE, false);

  print("**** INTERRUPTS ON ****\n");
  enable_interrupts();

  // LED should keep on blinking all the time
  while (true) {
    if (power_save_status == POWER_SAVE_STATUS_DISABLED) {
      #ifdef DEBUG_FAULTS
      if (fault_status == FAULT_STATUS_NONE) {
      #endif
        // useful for debugging, fade breaks = panda is overloaded
        for (uint32_t fade = 0U; fade < MAX_LED_FADE; fade += 1U) {
          led_set(LED_RED, true);
          delay(fade >> 4);
          led_set(LED_RED, false);
          delay((MAX_LED_FADE - fade) >> 4);
        }

        for (uint32_t fade = MAX_LED_FADE; fade > 0U; fade -= 1U) {
          led_set(LED_RED, true);
          delay(fade >> 4);
          led_set(LED_RED, false);
          delay((MAX_LED_FADE - fade) >> 4);
        }

      #ifdef DEBUG_FAULTS
      } else {
          led_set(LED_RED, 1);
          delay(512000U);
          led_set(LED_RED, 0);
          delay(512000U);
        }
      #endif
    } else {
      __WFI();
      SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    }
  }

  return 0;
}
