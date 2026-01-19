void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  UNUSED(data);
  UNUSED(len);
}

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  unsigned int resp_len = 0;

  switch (req->request) {
    // **** 0xc1: get hardware type
    case 0xc1:
      resp[0] = hw_type;
      resp_len = 1;
      break;

    // **** 0xd1: enter bootloader mode
    case 0xd1:
      switch (req->param1) {
        case 0:
          print("-> entering bootloader\n");
          enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
          NVIC_SystemReset();
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

    // **** 0xd3/0xd4: signature bytes
    case 0xd3:
    case 0xd4: {
      uint8_t offset = (req->request == 0xd3) ? 0U : 64U;
      resp_len = 64U;
      char *code = (char *)_app_start;
      int code_len = _app_start[0];
      (void)memcpy(resp, &code[code_len + offset], resp_len);
      break;
    }

    // **** 0xd6: get version
    case 0xd6:
      (void)memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = sizeof(gitversion) - 1U;
      break;

    // **** 0xd8: reset ST
    case 0xd8:
      NVIC_SystemReset();
      break;

    // **** 0xdd: get healthpacket and CANPacket versions
    case 0xdd:
      resp[0] = HEALTH_PACKET_VERSION;
      resp[1] = CAN_PACKET_VERSION;
      resp[2] = CAN_HEALTH_PACKET_VERSION;
      resp_len = 3U;
      break;

    // **** 0xb3: set motor speeds
    case 0xb3:
      rpml = (int16_t)req->param1;
      rpmr = (int16_t)req->param2;
      break;

    // **** 0xb4: enable/disable motors
    case 0xb4:
      enable_motors = (req->param1 == 1U);
      if (enable_motors == 0) {
        rpml = 0;
        rpmr = 0;
      }
      break;

    default:
      // Ignore unhandled requests
      break;
  }
  return resp_len;
}
