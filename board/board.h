// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/common.h"
int usb_power_mode = USB_POWER_NONE;

// ///// Function definitions ///// //
typedef void (*board_init)(void);
typedef void (*board_enable_can_transciever)(uint8_t transciever, bool enabled);
typedef void (*board_enable_can_transcievers)(bool enabled);
typedef void (*board_set_led)(uint8_t color, bool enabled);
typedef void (*board_set_usb_power_mode)(uint8_t mode);
typedef void (*board_set_esp_gps_mode)(uint8_t mode);
typedef void (*board_set_can_mode)(uint8_t mode);

typedef struct {
  const char *board_type;
  const uint8_t num_transcievers;
  board_init init;
  board_enable_can_transciever enable_can_transciever;
  board_enable_can_transcievers enable_can_transcievers;
  board_set_led set_led;
  board_set_usb_power_mode set_usb_power_mode;
  board_set_esp_gps_mode set_esp_gps_mode;
  board_set_can_mode set_can_mode;
} board;

// ///// Board definition ///// //
#ifdef PANDA
  #include "boards/white.h"
  #include "boards/grey.h"
  #include "boards/black.h"
#else
  #include "boards/pedal.h"
#endif

// ///// Board detection ///// //
#define PANDA_TYPE_UNSUPPORTED 0
#define PANDA_TYPE_WHITE 1
#define PANDA_TYPE_GREY 2
#define PANDA_TYPE_BLACK 3
#define PANDA_TYPE_PEDAL 4

int panda_type = PANDA_TYPE_UNSUPPORTED;
const board *current_board;

void detect_board_type(void){
  #ifdef PANDA
    // SPI lines floating: white (TODO: is this reliable?)
    if((detect_with_pull(GPIOA, 4, PULL_DOWN) | detect_with_pull(GPIOA, 5, PULL_DOWN) | detect_with_pull(GPIOA, 6, PULL_DOWN) | detect_with_pull(GPIOA, 7, PULL_DOWN))){
      panda_type = PANDA_TYPE_WHITE;
      current_board = &board_white;
    } else if(detect_with_pull(GPIOA, 13, PULL_DOWN)) { // Rev AB deprecated, so no pullup means black. In REV C, A13 is pulled up to 5V with a 10K
      panda_type = PANDA_TYPE_GREY;
      current_board = &board_grey;
    } else {
      panda_type = PANDA_TYPE_BLACK;
      current_board = &board_black;
    }
  #else
    #ifdef PEDAL
      panda_type = PANDA_TYPE_PEDAL;
      current_board = &board_pedal;
    #else 
      panda_type = PANDA_TYPE_UNSUPPORTED;
      puts("Panda is UNSUPPORTED!\n");
    #endif
  #endif
}


// ///// Configuration detection ///// //
bool has_external_debug_serial = 0;
bool is_entering_bootmode = 0;

void detect_configuration(void){
  // detect if external serial debugging is present
  has_external_debug_serial = detect_with_pull(GPIOA, 3, PULL_DOWN);

  #ifdef PANDA
    // check if the ESP is trying to put me in boot mode
    is_entering_bootmode = !detect_with_pull(GPIOB, 0, PULL_UP);
  #else
    is_entering_bootmode = 0;
  #endif
}

