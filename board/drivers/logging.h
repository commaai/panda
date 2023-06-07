
// Flash is writable in 32-byte lines, this struct is designed to fit in two lines.
// This also matches the USB transfer size.
typedef struct __attribute__((packed)) log_t {
  uint16_t id;
  timestamp_t timestamp;
  uint32_t uptime;
  char msg[50];
} log_t;

#define BANK_SIZE 0x20000U
#define BANK_LOG_SIZE (BANK_SIZE / sizeof(log_t))
#define LOG_SIZE (BANK_LOG_SIZE * 2U)
#define IS_EMPTY(log_ptr) (*(uint64_t *) (log_ptr) == 0xFFFFFFFFFFFFFFFFU)
#define LOGGING_NEXT_ID(id) ((id + 1U) % 0xFFFEU)
#define LOGGING_NEXT_INDEX(index) ((index + 1U) % LOG_SIZE)

struct logging_state_t {
  uint16_t read_index;
  uint16_t write_index;
  uint16_t last_id;
};
struct logging_state_t log_state = { 0 };
log_t *log_arr = (log_t *) LOGGING_FLASH_BASE_A;

void logging_erase_bank(uint8_t flash_sector) {
  print("erasing bank "); puth(flash_sector); print("\n");
  flash_unlock();
  flash_erase_sector(flash_sector);
  flash_lock();
}

void logging_erase(void) {
  logging_erase_bank(LOGGING_FLASH_SECTOR_A);
  logging_erase_bank(LOGGING_FLASH_SECTOR_B);
  log_state.read_index = 0U;
  log_state.write_index = 0U;
}

void logging_find_read_index(void) {
  // Figure out the read index by the last empty slot
  log_state.read_index = 0xFFFFU;
  for (uint16_t i = 0U; i < LOG_SIZE; i++) {
    if (log_arr[i].id == 0xFFFFU) {
      log_state.read_index = LOGGING_NEXT_INDEX(i);
    }
  }
}

void logging_init(void) {
  COMPILE_TIME_ASSERT(sizeof(log_t) == 64U);
  COMPILE_TIME_ASSERT(LOGGING_FLASH_BASE_A + BANK_SIZE == LOGGING_FLASH_BASE_B);

  // Make sure all empty-ID logs are fully empty
  for (uint16_t i = 0U; i < LOG_SIZE; i++) {
    if (log_arr[i].id == 0xFFFFU && !IS_EMPTY(&log_arr[i])) {
      logging_erase();
      break;
    }
  }

  logging_find_read_index();

  // At initialization, the read index should always be at the beginning of a bank
  // If not, clean slate
  if ((log_state.read_index != 0U) && (log_state.read_index != BANK_LOG_SIZE)) {
    logging_erase();
  }

  // Figure out the write index
  log_state.write_index = log_state.read_index;
  log_state.last_id = log_arr[log_state.write_index].id - 1U;
  for (uint16_t i = 0U; i < LOG_SIZE; i++) {
    if (log_arr[log_state.write_index].id == 0xFFFFU) {
      // Found the first empty slot after the read pointer
      break;
    } else if (log_arr[log_state.write_index].id != LOGGING_NEXT_ID(log_state.last_id)) {
      // Discontinuity in the index, shouldn't happen!
      logging_erase();
      break;
    }

    log_state.last_id = log_arr[log_state.write_index].id;
    log_state.write_index = LOGGING_NEXT_INDEX(log_state.write_index);
  }
}

void logging_tick(void) {
  flush_write_buffer();
}

void log(const char* msg){
  log_t log = {0};
  log.id = LOGGING_NEXT_ID(log_state.last_id);
  log_state.last_id = log.id;
  log.uptime = uptime_cnt;
  if (current_board->has_rtc_battery) {
    log.timestamp = rtc_get_time();
  }

  for(uint8_t i = 0; i < sizeof(log.msg); i++) {
    log.msg[i] = msg[i];
    if (msg[i] == '\0') {
      break;
    }
  }

  // If we are at the beginning of a bank, erase it first and move the read pointer if needed
  if (log_state.write_index == 0U) {
    logging_erase_bank(LOGGING_FLASH_SECTOR_A);
    if (log_state.read_index < BANK_LOG_SIZE && log_state.read_index != 0U) {
      log_state.read_index = BANK_LOG_SIZE;
    }
  } else if (log_state.write_index == BANK_LOG_SIZE) {
    // beginning to write in bank B
    logging_erase_bank(LOGGING_FLASH_SECTOR_B);
    if (log_state.read_index > BANK_LOG_SIZE) {
      log_state.read_index = 0U;
    }
  }

  // Write!
  flash_unlock();
  for (uint8_t i = 0U; i < sizeof(log_t) / sizeof(uint32_t); i++) {
    flash_write_word(((uint32_t *) &log_arr[log_state.write_index]) + i, ((uint32_t *) &log)[i]);
  }
  flash_lock();

  // Update the write index
  log_state.write_index = LOGGING_NEXT_INDEX(log_state.write_index);
}

bool logging_read(uint8_t *buffer) {
  if (log_state.read_index == log_state.write_index) {
    return false;
  }

  // Read the log
  memcpy(buffer, &log_arr[log_state.read_index], sizeof(log_t));

  // Update the read index
  log_state.read_index = LOGGING_NEXT_INDEX(log_state.read_index);

  return true;
}
