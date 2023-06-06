
// Flash is writable in 32-byte lines, this struct is designed to fit in two lines

typedef struct __attribute__((packed)) log_t {
  uint16_t id;
  uint32_t timestamp;
  uint32_t uptime;
  char msg[54];
} log_t;

typedef struct bank_t {
  uint8_t sector;
  uint32_t *base;

  uint16_t index;
  bool valid;
} bank_t;

#define BANK_SIZE 0x20000U
#define BANK_LOG_SIZE (BANK_SIZE / sizeof(log_t))

#define BANK_A 0x0U
#define BANK_B 0x1U
uint8_t logging_bank = BANK_A;
bank_t log_bank[2];

#define OTHER_BANK(x) (x == BANK_A ? BANK_B : BANK_A)
#define GET_LOG(bank, i) (&((log_t *) (bank)->base)[i])
#define LAST_LOG(bank) GET_LOG((bank), (bank)->index - 1U)

void logging_erase_bank(bank_t *bank) {
  flash_unlock();
  flash_erase_sector(bank->sector);
  flash_lock();

  bank->index = 0;
  bank->valid = true;
}

void logging_init_bank(bank_t *bank) {
  uint32_t last_id = ((log_t *) bank->base)->id;

  bank->valid = true;
  for (uint32_t i = 1; i < (BANK_SIZE / sizeof(log_t)); i++) {
    log_t* log = &((log_t *) bank->base)[i];

    if ((log->id != 0xFFFFU) && ((log->id != last_id + 1U) || (last_id == 0xFFFFU))) {
      bank->valid = false;
    }

    if (log->id == 0xFFFFU) {
      // Make sure the whole log is empty
      for (uint8_t j = 0; j < sizeof(log_t) / sizeof(uint32_t); j++) {
        if (((uint32_t *) log)[j] != 0xFFFFFFFFU) {
          bank->valid = false;
          break;
        }
      }
    } else {
      bank->index = i + 1U;
    }

    if(!bank->valid) {
      break;
    }

    last_id = log->id;
  }

  if(!bank->valid) {
    logging_erase_bank(bank);
  }
}

void logging_init(void) {
  COMPILE_TIME_ASSERT(sizeof(log_t) == 64U);

  // Initialize banks
  log_bank[BANK_A].sector = LOGGING_FLASH_SECTOR_A;
  log_bank[BANK_B].sector = LOGGING_FLASH_SECTOR_B;
  log_bank[BANK_A].base = (uint32_t *) LOGGING_FLASH_BASE_A;
  log_bank[BANK_B].base = (uint32_t *) LOGGING_FLASH_BASE_B;
  logging_init_bank(&log_bank[BANK_A]);
  logging_init_bank(&log_bank[BANK_B]);

  // Pick the bank to log to
  if (log_bank[BANK_A].index == 0U && log_bank[BANK_B].index == 0U) {
    // Both are empty
    logging_bank = BANK_A;
  } else {
    if (log_bank[BANK_A].index == 0U) {
      logging_bank = BANK_B;
    } else if (log_bank[BANK_B].index == 0U) {
      logging_bank = BANK_A;
    } else {
      // Both are non-empty: select the one with the highest ID.
      // This isn't always the best possible solution, but good enough.
      if (LAST_LOG(&log_bank[BANK_A])->id > LAST_LOG(&log_bank[BANK_B])->id) {
        logging_bank = BANK_A;
      } else {
        logging_bank = BANK_B;
      }

      // If non-continuous, erase the other bank
      if ((log_bank[OTHER_BANK(logging_bank)].index != BANK_LOG_SIZE) || (LAST_LOG(&log_bank[OTHER_BANK(logging_bank)])->id != GET_LOG(&log_bank[logging_bank], 0)->id - 1U)) {
        logging_erase_bank(&log_bank[OTHER_BANK(logging_bank)]);
      }
    }
  }
}

void logging_swap_banks(void) {
  if (logging_bank == BANK_A) {
    logging_bank = BANK_B;
  } else {
    logging_bank = BANK_A;
  }
  logging_erase_bank(&log_bank[logging_bank]);
}

void logging_tick(void) {
  flush_write_buffer();
}

void log(char* msg){
  log_t log = {0};
  log.id = LAST_LOG(&log_bank[logging_bank])->id + 1U;
  log.uptime = uptime_cnt;
  if (current_board->has_rtc_battery) {
    log.timestamp = rtc_get_raw_time();
  } else {
    log.timestamp = 0;
  }

  for(uint8_t i = 0; i < sizeof(log.msg); i++) {
    log.msg[i] = msg[i];
    if (msg[i] == '\0') {
      break;
    }
  }

  if (log_bank[logging_bank].index == BANK_LOG_SIZE) {
    logging_swap_banks();
  }

  void *address = log_bank[logging_bank].base + (log_bank[logging_bank].index * sizeof(log_t));

  flash_unlock();
  flash_write_word(address, ((uint32_t *) &log)[0]);
  flash_write_word(address + sizeof(uint32_t), ((uint32_t *) &log)[1]);
  flash_lock();

  log_bank[logging_bank].index++;
}