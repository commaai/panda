bool bootkick_ign_prev = false;
BootState boot_state = BOOT_BOOTKICK;
uint8_t bootkick_harness_status_prev = HARNESS_STATUS_NC;

bool waiting_to_boot = false;
uint8_t boot_reset_countdown = 0;
uint32_t waiting_to_boot_count = 0;
bool bootkick_reset_triggered = false;

void bootkick_tick(bool ignition, bool recent_heartbeat) {
  const bool harness_inserted = (harness.status != bootkick_harness_status_prev) && (harness.status != HARNESS_STATUS_NC);
  if ((ignition && !bootkick_ign_prev) || harness_inserted) {
    // bootkick on rising edge of ignition or harness insertion
    boot_state = BOOT_BOOTKICK;
  } else if (recent_heartbeat) {
    // disable bootkick once openpilot is up
    boot_state = BOOT_STANDBY;
  } else {

  }

  // ensure SOM boots
  if ((boot_state == BOOT_BOOTKICK) && !waiting_to_boot) {
    waiting_to_boot = true;
    waiting_to_boot_count = 0;
  }
  if (waiting_to_boot) {
    if (current_board->read_som_gpio()) {
      waiting_to_boot = false;
    } else if (waiting_to_boot_count == 30U) {
      boot_reset_countdown = 5U;
    } else {

    }
    waiting_to_boot_count += 1U;
  }

  if (boot_reset_countdown > 0U) {
    boot_reset_countdown--;
    boot_state = BOOT_RESET;
    bootkick_reset_triggered = true;
  } else if (boot_state == BOOT_RESET) {
    boot_state = BOOT_BOOTKICK;
  }

  // update state
  bootkick_ign_prev = ignition;
  bootkick_harness_status_prev = harness.status;
  current_board->set_bootkick(boot_state);
}
