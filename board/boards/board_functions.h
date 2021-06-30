// ///// Board functions ///// //
// TODO: Make these config options in the board struct
bool board_has_gps(void) {
  return ((hw_type == HW_TYPE_GREY_PANDA) || (hw_type == HW_TYPE_BLACK_PANDA) || (hw_type == HW_TYPE_UNO));
}

bool board_has_gmlan(void) {
  return ((hw_type == HW_TYPE_WHITE_PANDA) || (hw_type == HW_TYPE_GREY_PANDA));
}

bool board_has_obd(void) {
  return ((hw_type == HW_TYPE_BLACK_PANDA) || (hw_type == HW_TYPE_UNO) || (hw_type == HW_TYPE_DOS));
}

bool board_has_lin(void) {
  return ((hw_type == HW_TYPE_WHITE_PANDA) || (hw_type == HW_TYPE_GREY_PANDA));
}

bool board_has_rtc(void) {
  return ((hw_type == HW_TYPE_UNO) || (hw_type == HW_TYPE_DOS));
}
