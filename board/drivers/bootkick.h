bool bootkick_ign_prev = false;
BootState boot_state = BOOT_BOOTKICK;
uint8_t bootkick_harness_status_prev = HARNESS_STATUS_NC;

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

  // update state
  bootkick_ign_prev = ignition;
  bootkick_harness_status_prev = harness.status;
  current_board->set_bootkick(boot_state);
}
