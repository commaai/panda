// Body V2 minimal comms handler

void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  UNUSED(data);
  UNUSED(len);
}

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  unsigned int resp_len = 0;

  switch (req->request) {
    // **** 0xc1: get hardware type (REQUIRED)
    case 0xc1:
      resp[0] = hw_type;
      resp_len = 1;
      break;
    
    // **** 0xd1: enter bootloader mode (REQUIRED for flashing)
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
    
    case 0xd6:  // get version
      (void)memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = sizeof(gitversion) - 1U;
      break;

    // **** 0xd8: reset ST
    case 0xd8:
      NVIC_SystemReset();
      break;

    default:
      // Ignore unhandled requests
      break;
  }
  return resp_len;
}