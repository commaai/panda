#pragma once

// SD card CAN replay support for panda jungle v2.
//
// SD card binary format (raw sectors, no filesystem):
//
// Sector 0 — header (512 bytes):
//   bytes  0-7:  magic "PNDREPLY"
//   bytes  8-11: uint32_t num_records
//   bytes 12-15: uint32_t record_size  (must equal sizeof(sd_can_record_t) = 20)
//   bytes 16-19: uint32_t format_version  (= 1)
//   remaining bytes: zero-padded
//
// Sectors 1+ — records (20 bytes each):
//   struct sd_can_record_t (see below)
//
// Records must be sorted by mono_time_us ascending.
// Use board/jungle/scripts/provision_sd.py to write this format.

#define SD_REPLAY_MAGIC        "PNDREPLY"
#define SD_REPLAY_MAGIC_LEN    8U
#define SD_REPLAY_FORMAT_VER   1U
#define SD_RECORD_SIZE         20U
#define SD_HEADER_SECTOR       0U
#define SD_DATA_START_SECTOR   1U

// Number of sectors to read at once into idmabuf (idmabuf = 32 * 512 = 16384 bytes)
#define SD_READ_SECTORS        32U
#define SD_RECORDS_PER_BUF     (SD_READ_SECTORS * SDMMC_BLOCK_SIZE / SD_RECORD_SIZE)  // = 819

typedef struct __attribute__((packed)) {
  uint32_t mono_time_us;  // microseconds since replay start (relative, starts at 0)
  uint32_t addr;          // CAN address
  uint8_t  bus;           // CAN bus number (0-2)
  uint8_t  len;           // data byte length (0-8)
  uint8_t  data[8];       // CAN data payload (zero-padded if len < 8)
  uint16_t pad;           // reserved, must be 0
} sd_can_record_t;

typedef struct __attribute__((packed)) {
  uint8_t  magic[SD_REPLAY_MAGIC_LEN];
  uint32_t num_records;
  uint32_t record_size;
  uint32_t format_version;
} sd_replay_header_t;

typedef enum {
  SD_REPLAY_IDLE    = 0U,
  SD_REPLAY_ACTIVE  = 1U,
  SD_REPLAY_DONE    = 2U,
  SD_REPLAY_ERROR   = 3U,
} sd_replay_state_t;

sd_replay_state_t sd_replay_state    = SD_REPLAY_IDLE;
uint32_t sd_replay_total_records     = 0U;
uint32_t sd_replay_current_record    = 0U;

static uint32_t sd_replay_start_us   = 0U;
static uint32_t sd_buf_record_idx    = 0U;   // index within current idmabuf
static uint32_t sd_buf_record_count  = 0U;   // valid records in current idmabuf
static uint32_t sd_next_sector       = 0U;   // next SD sector to read

static bool sd_replay_fill_buffer(void) {
  uint32_t sectors_remaining = (sd_replay_total_records * SD_RECORD_SIZE + SDMMC_BLOCK_SIZE - 1U) / SDMMC_BLOCK_SIZE
                               - (sd_next_sector - SD_DATA_START_SECTOR);
  if (sectors_remaining == 0U) {
    sd_buf_record_count = 0U;
    return false;
  }
  uint32_t sectors_to_read = MIN(SD_READ_SECTORS, sectors_remaining);
  sd_error err = sdmmc_read_idma(idmabuf, sd_next_sector, sectors_to_read);
  if (err != sd_err_ok) {
    sd_replay_state = SD_REPLAY_ERROR;
    return false;
  }
  sd_next_sector += sectors_to_read;

  // how many complete records fit in the bytes we just read
  uint32_t bytes_read = sectors_to_read * SDMMC_BLOCK_SIZE;
  uint32_t records_in_buf = bytes_read / SD_RECORD_SIZE;
  uint32_t records_left = sd_replay_total_records - sd_replay_current_record;
  sd_buf_record_count = MIN(records_in_buf, records_left);
  sd_buf_record_idx = 0U;
  return true;
}

void sd_replay_init(void) {
  // Read header from sector 0
  sd_error err = sdmmc_read_idma(idmabuf, SD_HEADER_SECTOR, 1U);
  if (err != sd_err_ok) {
    return;
  }

  sd_replay_header_t hdr;
  (void)memcpy(&hdr, idmabuf, sizeof(hdr));

  if ((memcmp(hdr.magic, SD_REPLAY_MAGIC, SD_REPLAY_MAGIC_LEN) != 0) ||
      (hdr.record_size != SD_RECORD_SIZE) ||
      (hdr.format_version != SD_REPLAY_FORMAT_VER) ||
      (hdr.num_records == 0U)) {
    print("SD replay: no valid replay image found\n");
    return;
  }

  sd_replay_total_records = hdr.num_records;
  sd_replay_current_record = 0U;
  sd_replay_state = SD_REPLAY_IDLE;
  print("SD replay: found "); puth(sd_replay_total_records); print(" records\n");
}

void sd_replay_start(void) {
  if (sd_replay_total_records == 0U) {
    print("SD replay: no records loaded\n");
    return;
  }
  sd_replay_current_record = 0U;
  sd_buf_record_idx        = 0U;
  sd_buf_record_count      = 0U;
  sd_next_sector           = SD_DATA_START_SECTOR;
  sd_replay_start_us       = microsecond_timer_get();
  sd_replay_state          = SD_REPLAY_ACTIVE;
  print("SD replay: started\n");
}

void sd_replay_stop(void) {
  sd_replay_state = SD_REPLAY_IDLE;
  print("SD replay: stopped\n");
}

// Call from main loop when sd_replay_state == SD_REPLAY_ACTIVE.
void sd_replay_tick(void) {
  // Refill buffer when empty
  if (sd_buf_record_idx >= sd_buf_record_count) {
    if (!sd_replay_fill_buffer()) {
      if (sd_replay_state == SD_REPLAY_ACTIVE) {
        sd_replay_state = SD_REPLAY_DONE;
        print("SD replay: done\n");
      }
      return;
    }
  }

  uint32_t now_us = microsecond_timer_get();
  uint32_t elapsed_us = now_us - sd_replay_start_us;

  // Send all records whose scheduled time has arrived
  while (sd_buf_record_idx < sd_buf_record_count) {
    sd_can_record_t *rec = (sd_can_record_t *)(idmabuf + (sd_buf_record_idx * SD_RECORD_SIZE));

    if (elapsed_us < rec->mono_time_us) {
      break;  // not yet time for this record
    }

    if (rec->bus < PANDA_CAN_CNT) {
      uint8_t dlc = rec->len;  // for standard CAN, len == DLC for 0-8
      if (dlc > 8U) {
        dlc = 8U;
      }

      CANPacket_t pkt = {0};
      pkt.fd       = 0U;
      pkt.bus      = rec->bus;
      pkt.data_len_code = dlc;
      pkt.returned = 0U;
      pkt.rejected = 0U;
      pkt.extended = (rec->addr >= 0x800U) ? 1U : 0U;
      pkt.addr     = rec->addr;
      (void)memcpy(pkt.data, rec->data, dlc);
      can_set_checksum(&pkt);
      can_send(&pkt, pkt.bus, true);
    }

    sd_buf_record_idx++;
    sd_replay_current_record++;

    if (sd_replay_current_record >= sd_replay_total_records) {
      sd_replay_state = SD_REPLAY_DONE;
      print("SD replay: done\n");
      return;
    }

    // Update elapsed time for next record comparison
    now_us = microsecond_timer_get();
    elapsed_us = now_us - sd_replay_start_us;
  }

  // When current buffer is exhausted, top it up on the next tick
}
