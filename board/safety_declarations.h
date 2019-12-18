const int MAX_BAD_COUNTERS = 5;
const uint8_t MAX_MISSED_MSGS = 10U;

// sample struct that keeps 3 samples in memory
struct sample_t {
  int values[6];
  int min;
  int max;
} sample_t_default = {{0}, 0, 0};

// safety code requires floats
struct lookup_t {
  float x[3];
  float y[3];
};

typedef struct {
  int addr;
  int bus;
} AddrBus;

// TODO: add fields for frequency checks
typedef struct {
  // addr is array because some cars have mutually exclusive addresses for the same function
  // 2 is currently the max number of mutually exclusive messages; make it larger if needed
  const int addr[2];
  const uint8_t addr_len;
  const int bus;
  const bool check_checksum;
  bool valid_checksum;
  const bool check_counter;
  const uint8_t max_counter;
  uint8_t last_counter;
  int bad_counters;
  const uint32_t expected_timestep;  // us
  uint32_t last_timestamp;  // us
  bool lagging;
} AddrCheckStruct;

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len);
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last);
int to_signed(int d, int bits);
void update_sample(struct sample_t *sample, int sample_new);
bool max_limit_check(int val, const int MAX, const int MIN);
bool dist_to_meas_check(int val, int val_last, struct sample_t *val_meas,
  const int MAX_RATE_UP, const int MAX_RATE_DOWN, const int MAX_ERROR);
bool driver_limit_check(int val, int val_last, struct sample_t *val_driver,
  const int MAX, const int MAX_RATE_UP, const int MAX_RATE_DOWN,
  const int MAX_ALLOWANCE, const int DRIVER_FACTOR);
bool rt_rate_limit_check(int val, int val_last, const int MAX_RT_DELTA);
float interpolate(struct lookup_t xy, float x);
bool addr_allowed(int addr, int bus, const AddrBus addr_list[], int len);
int get_addr_check_index(CAN_FIFOMailBox_TypeDef *to_push, AddrCheckStruct addr_list[], int len);
bool is_addr_valid(AddrCheckStruct addr_list[], int index);

typedef void (*safety_hook_init)(int16_t param);
typedef void (*rx_hook)(CAN_FIFOMailBox_TypeDef *to_push);
typedef int (*tx_hook)(CAN_FIFOMailBox_TypeDef *to_send);
typedef int (*tx_lin_hook)(int lin_num, uint8_t *data, int len);
typedef int (*fwd_hook)(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd);

typedef struct {
  safety_hook_init init;
  rx_hook rx;
  tx_hook tx;
  tx_lin_hook tx_lin;
  fwd_hook fwd;
  AddrCheckStruct *addr_check;
  uint8_t addr_check_len;
} safety_hooks;

// This can be set by the safety hooks
bool controls_allowed = false;
bool relay_malfunction = false;
bool gas_interceptor_detected = false;
int gas_interceptor_prev = 0;

// This is set by USB command 0xdf
bool long_controls_allowed = true;

// time since safety mode has been changed
uint32_t safety_mode_cnt = 0U;
// allow 1s of transition timeout after relay changes state before assessing malfunctioning
const uint32_t RELAY_TRNS_TIMEOUT = 1U;

// avg between 2 tracks
#define GET_INTERCEPTOR(msg) (((GET_BYTE((msg), 0) << 8) + GET_BYTE((msg), 1) + ((GET_BYTE((msg), 2) << 8) + GET_BYTE((msg), 3)) / 2 ) / 2)
