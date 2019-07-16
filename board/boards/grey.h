// ////////// //
// Grey Panda //
// ////////// //

// Most hardware functionality is similar to white panda
const board board_grey = {
  .board_type = "Grey",
  .init = white_init,
  .enable_can_transciever = white_enable_can_transciever,
  .enable_can_transcievers = white_enable_can_transcievers,
  .set_led = white_set_led,
  .set_usb_power_mode = white_set_usb_power_mode,
  .set_esp_gps_mode = white_set_esp_gps_mode,
  .set_can_mode = white_set_can_mode
};