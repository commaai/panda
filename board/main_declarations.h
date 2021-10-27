// ******************** Prototypes ********************
void puts(const char *a);
void puth(unsigned int i);
void puth2(unsigned int i);
void puth4(unsigned int i);
typedef struct board board;
typedef struct harness_configuration harness_configuration;
void can_flip_buses(uint8_t bus1, uint8_t bus2);
void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
typedef struct {
  unsigned char reserved : 1;
  unsigned char bus : 3;
  unsigned char dlc : 4;
  unsigned char rejected : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;  
  unsigned int addr : 29;
  unsigned char data[CANPACKET_DATA_SIZE_MAX];
} __attribute__((packed, aligned(1))) CANPacket_t;
// ********************* Globals **********************
uint8_t hw_type = 0;
const board *current_board;
bool is_enumerated = 0;
uint32_t uptime_cnt = 0;
bool green_led_enabled = false;

// heartbeat state
uint32_t heartbeat_counter = 0;
bool heartbeat_lost = false;
bool heartbeat_disabled = false; // set over USB

// siren state
bool siren_enabled = false;
uint32_t siren_countdown = 0; // siren plays while countdown > 0
uint32_t controls_allowed_countdown = 0;

