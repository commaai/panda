# PANDA BOUNTY #2171 CODING STANDARDS

**Document Version**: 1.0  
**Date**: 2025-10-20  
**Target Project**: comma.ai Panda Firmware Refactoring  
**Scope**: STM32H7-based automotive firmware development  

---

## TABLE OF CONTENTS

1. [Executive Summary](#executive-summary)
2. [C Programming Standards](#c-programming-standards)
3. [MISRA C Compliance](#misra-c-compliance)
4. [Panda-Specific Standards](#panda-specific-standards)
5. [Header File Architecture](#header-file-architecture)
6. [Refactoring Standards](#refactoring-standards)
7. [Security Standards](#security-standards)
8. [Quality Assurance](#quality-assurance)
9. [Examples and Patterns](#examples-and-patterns)
10. [Testing Requirements](#testing-requirements)

---

## EXECUTIVE SUMMARY

This document establishes coding standards for the Panda Bounty #2171 refactoring project, which involves converting header-only implementations to proper declaration/implementation separation. The standards ensure safety-critical automotive firmware quality, MISRA C compliance, and consistency with comma.ai's existing codebase patterns.

### Key Objectives
- **Safety-Critical Quality**: Maintain automotive-grade reliability standards
- **MISRA C Compliance**: Follow established automotive coding standards
- **Maintainability**: Separate interface from implementation
- **Consistency**: Align with existing panda codebase conventions
- **Security**: Prevent buffer overflows and undefined behavior

---

## C PROGRAMMING STANDARDS

### 1. General Coding Principles

#### 1.1 Code Structure
- **Header Separation**: All function implementations MUST be in `.c` files
- **Interface Declaration**: All public interfaces MUST be declared in header files
- **Single Responsibility**: Each function should have one clear purpose
- **Minimal Dependencies**: Minimize header inclusion chains

#### 1.2 Language Standards
- **Standard**: GNU C11 (`-std=gnu11`)
- **Extensions**: Limited use of GNU extensions (e.g., `__typeof__` for macros)
- **Compiler**: ARM GCC cross-compiler (`arm-none-eabi-gcc`)

### 2. Naming Conventions

Based on analysis of the existing panda codebase:

#### 2.1 Function Names
```c
// Pattern: action_object_context() - all lowercase with underscores
void set_safety_mode(uint16_t mode, uint16_t param);
bool can_tx_check_min_slots_free(uint32_t min);
void enable_can_transceivers(bool enabled);
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last);
uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly);
```

**Rules:**
- Use descriptive verb-noun combinations
- All lowercase with underscores
- Boolean functions often start with `is_`, `can_`, `has_`
- Getter functions start with `get_`
- Setter functions start with `set_`

#### 2.2 Variable Names
```c
// Global variables
extern uint32_t safety_tx_blocked;
extern bool can_silent;
extern can_health_t can_health[PANDA_CAN_CNT];

// Local variables
uint16_t mode_copy = mode;
uint8_t crc = 0xFFU;
uart_ring *ring;
```

**Rules:**
- All lowercase with underscores
- Descriptive names, avoid abbreviations unless standard
- Global variables use full descriptive names
- Boolean variables use descriptive names (`enabled`, `found`, `active`)

#### 2.3 Constants and Macros
```c
// Constants
#define PANDA_CAN_CNT 3U
#define CAN_INIT_TIMEOUT_MS 500U
#define USBPACKET_MAX_SIZE 0x40U
#define FIFO_SIZE_INT 0x400U

// Hardware types
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_RED_PANDA 7U
#define HW_TYPE_TRES 9U
#define HW_TYPE_CUATRO 10U

// Power states
#define POWER_SAVE_STATUS_DISABLED 0
#define POWER_SAVE_STATUS_ENABLED 1
```

**Rules:**
- ALL_CAPS with underscores
- Use descriptive prefixes for grouping (e.g., `HW_TYPE_`, `CAN_MODE_`)
- Numeric constants include type suffix (`U` for unsigned)
- Hex values use uppercase (`0xDDEEU`)

#### 2.4 Type Names
```c
// Structures with _t suffix
typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

typedef struct {
  uint8_t bus_lookup;
  uint8_t can_num_lookup;
  int8_t forwarding_bus;
  uint32_t can_speed;
  bool canfd_enabled;
} bus_config_t;

// Function pointer types
typedef void (*board_init)(void);
typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);
```

**Rules:**
- Structure types use `snake_case` with optional `_t` suffix
- Function pointer types use descriptive names with `_t` suffix
- Enum types use `snake_case`

### 3. File and Directory Organization

#### 3.1 File Naming
```
board/
├── config.h                    # Central configuration
├── main.c                      # Main application entry
├── main.h                      # Main declarations
├── can.h                       # CAN interface definitions
├── health.h                    # Health monitoring
├── crc.h                       # CRC utility declarations
├── crc.c                       # CRC utility implementations
├── utils.h                     # Utility macros and declarations
├── drivers/
│   ├── can_common.h            # CAN driver implementations
│   ├── can_common_declarations.h  # CAN driver declarations
│   ├── uart.h                  # UART driver implementations
│   ├── uart_declarations.h     # UART driver declarations
│   └── ...
└── stm32h7/
    ├── board.h                 # Platform-specific board definitions
    ├── llfdcan.h              # Low-level FDCAN driver
    ├── llfdcan_declarations.h  # Low-level FDCAN declarations
    └── ...
```

#### 3.2 Include Hierarchy
```c
// Main application file example
#include "board/config.h"        // Always first

#include "board/drivers/led.h"
#include "board/drivers/usb.h"
#include "board/early_init.h"

#include "opendbc/safety/safety.h"  // External dependencies

#include "board/health.h"
#include "board/can_comms.h"
```

**Rules:**
- `config.h` is always included first
- Group includes by category (drivers, external, application)
- Use relative paths from project root
- Include only what is needed

---

## MISRA C COMPLIANCE

### 1. MISRA C 2012 Standard

The panda firmware follows MISRA C 2012 automotive coding standard with specific suppressions.

#### 1.1 Enabled Rules
Based on `/home/dholzric/projects/comma/openpilot/panda/tests/misra/checkers.txt`:
- **245+ MISRA C 2012 rules actively checked**
- **Buffer safety**: Array bounds, pointer arithmetic
- **Type safety**: Implicit conversions, casts
- **Control flow**: Unreachable code, switch statements
- **Function safety**: Return values, parameter validation

#### 1.2 Approved Suppressions
From `/home/dholzric/projects/comma/openpilot/panda/tests/misra/suppressions.txt`:

```c
// Advisory: casting from void pointer to type pointer is ok
misra-c2012-11.4
misra-c2012-11.5

// Advisory: goto statements in accordance to 15.2 and 15.3
misra-c2012-15.1

// Advisory: union types can be used
misra-c2012-19.2

// Advisory: The # and ## preprocessor operators should not be used
misra-c2012-20.10
```

#### 1.3 MISRA Compliance in Code
```c
// Correct: Explicit cast with suppression comment
(void)put_char(ring, rcv);  // misra-c2012-17.7: cast to void is ok: debug function

// Correct: Proper type usage
uint32_t timeout = CAN_INIT_TIMEOUT_MS;

// Correct: Explicit numeric suffixes
#define PANDA_CAN_CNT 3U
#define CAN_INIT_TIMEOUT_MS 500U
```

### 2. Static Analysis Integration

#### 2.1 Cppcheck Integration
```bash
# From test_misra.sh
--enable=all --disable=unusedFunction --addon=misra 
-DSTM32H7 -DSTM32H725xx -I /board/stm32h7/inc/ /board/main.c
```

#### 2.2 Build-Time Checks
- **Compiler warnings as errors**: `-Werror`
- **Enhanced warnings**: `-Wall -Wextra -Wstrict-prototypes`
- **MISRA addon**: Integrated into build process

---

## PANDA-SPECIFIC STANDARDS

### 1. Platform Considerations

#### 1.1 STM32H7 Architecture
- **Target MCU**: STM32H725xx, STM32H735xx
- **Architecture**: ARM Cortex-M7 with FPU
- **Memory**: Flash at 0x8020000, SRAM constraints
- **Real-time**: Interrupt-driven, safety-critical timing

#### 1.2 Automotive Requirements
```c
// Safety-critical patterns
typedef enum {
  BOOT_STANDBY,
  BOOT_BOOTKICK,
  BOOT_RESET,
} BootState;

// Error handling pattern
int err = set_safety_hooks(mode_copy, param);
if (err == -1) {
  print("Error: safety set mode failed. Falling back to SILENT\n");
  mode_copy = SAFETY_SILENT;
  err = set_safety_hooks(mode_copy, 0U);
  // TERMINAL ERROR: we can't continue if SILENT safety mode isn't successfully set
  assert_fatal(err == 0, "Error: Failed setting SILENT mode. Hanging\n");
}
```

### 2. Memory Management

#### 2.1 Stack Usage
- **Minimal stack allocation**: Use static/global storage when possible
- **No dynamic allocation**: No malloc/free in firmware
- **Buffer management**: Pre-allocated ring buffers for communication

#### 2.2 Real-Time Constraints
```c
// Interrupt-safe patterns
typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

// Critical sections
#define ENTER_CRITICAL() __disable_irq()
#define EXIT_CRITICAL() __enable_irq()
```

### 3. Communication Protocols

#### 3.1 CAN Bus Standards
```c
// CAN configuration structure
typedef struct {
  uint8_t bus_lookup;
  uint8_t can_num_lookup;
  int8_t forwarding_bus;
  uint32_t can_speed;
  uint32_t can_data_speed;
  bool canfd_enabled;
  bool brs_enabled;
  bool canfd_non_iso;
} bus_config_t;
```

#### 3.2 USB Communication
```c
// USB packet constraints
#define USBPACKET_MAX_SIZE 0x40U
#define MAX_CAN_MSGS_PER_USB_BULK_TRANSFER 51U
```

---

## HEADER FILE ARCHITECTURE

### 1. Current Pattern Analysis

The panda codebase uses a dual-header pattern:

#### 1.1 Declaration Headers (`*_declarations.h`)
```c
// uart_declarations.h - Interface only
#pragma once

typedef struct uart_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  uint8_t *elems_tx;
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
} uart_ring;

// Function prototypes only
void debug_ring_callback(uart_ring *ring);
void uart_tx_ring(uart_ring *q);
bool get_char(uart_ring *q, char *elem);
void print(const char *a);
```

#### 1.2 Implementation Headers (`*.h`)
```c
// uart.h - Contains implementations
#include "uart_declarations.h"

static void puth4(unsigned int i) {
  puth((i >> 12U) & 0xFU);
  puth((i >> 8U) & 0xFU);
  puth((i >> 4U) & 0xFU);
  puth(i & 0xFU);
}

static void hexdump(const void *a, int l) {
  // Implementation...
}
```

### 2. Include Guard Standards

#### 2.1 Pragma Once
```c
// REQUIRED: All headers must use pragma once
#pragma once

// NOT used: Traditional include guards
// #ifndef HEADER_H
// #define HEADER_H
// #endif
```

#### 2.2 Header Dependencies
```c
// Minimal forward declarations
typedef struct board board;
typedef struct harness_configuration harness_configuration;

// Include only required headers
#include <stdint.h>
#include <stdbool.h>
#include "board/can.h"
```

### 3. Section Organization

#### 3.1 Standard Header Structure
```c
#pragma once

// System includes
#include <stdint.h>
#include <stdbool.h>

// Project includes  
#include "board/can.h"

// ***************************** Definitions *****************************
#define FIFO_SIZE_INT 0x400U

// ***************************** Type Definitions *****************************
typedef struct uart_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  // ...
} uart_ring;

// ***************************** Function Prototypes *****************************
void debug_ring_callback(uart_ring *ring);
void uart_tx_ring(uart_ring *q);

// ***************************** Global Variables *****************************
extern uart_ring *debug_ring;
```

#### 3.2 Comment Section Headers
```c
// Standard section headers used in panda codebase:
// ********************* Includes *********************
// ******************** Prototypes ********************
// ******************* Definitions ********************
// ********************* Globals **********************
// ***************************** Definitions *****************************
// ***************************** Function prototypes *****************************
// ************************* Low-level buffer functions *************************
// ************************ High-level debug functions **********************
```

---

## REFACTORING STANDARDS

### 1. Declaration/Implementation Separation

#### 1.1 Target Pattern for Bounty #2171

**BEFORE (current anti-pattern):**
```c
// crc.h - Implementation in header (BAD)
#pragma once

uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly) {
  uint8_t crc = 0xFFU;
  int i;
  int j;
  for (i = len - 1; i >= 0; i--) {
    crc ^= dat[i];
    for (j = 0; j < 8; j++) {
      if ((crc & 0x80U) != 0U) {
        crc = (uint8_t)((crc << 1) ^ poly);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
```

**AFTER (target pattern):**
```c
// crc.h - Declarations only (GOOD)
#pragma once

#include <stdint.h>

// ***************************** Function Prototypes *****************************
uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly);
```

```c
// crc.c - Implementations (GOOD)
#include "board/crc.h"

uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly) {
  uint8_t crc = 0xFFU;
  int i;
  int j;
  for (i = len - 1; i >= 0; i--) {
    crc ^= dat[i];
    for (j = 0; j < 8; j++) {
      if ((crc & 0x80U) != 0U) {
        crc = (uint8_t)((crc << 1) ^ poly);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
```

### 2. Function Prototype Formatting

#### 2.1 Standard Format
```c
// Return type on same line, parameters aligned
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool can_tx_check_min_slots_free(uint32_t min);
uint8_t calculate_checksum(const uint8_t *dat, uint32_t len);

// Long prototypes - break after parameter list
bool is_speed_valid(uint32_t speed, 
                   const uint32_t *all_speeds, 
                   uint8_t len);
```

#### 2.2 Parameter Documentation
```c
// Document complex parameters
void board_enable_can_transceiver(uint8_t transceiver,   // 0-based transceiver index
                                 bool enabled);         // true=enable, false=disable

// Simple parameters don't need documentation
void set_power_save_state(int state);
bool can_push(can_ring *q, const CANPacket_t *elem);
```

### 3. Include Statement Organization

#### 3.1 Standard Include Order
```c
// 1. Always include config.h first in .c files
#include "board/config.h"

// 2. Corresponding header file
#include "board/crc.h"

// 3. System headers
#include <stdint.h>
#include <stdbool.h>

// 4. Platform headers  
#include "board/stm32h7/inc/stm32h7xx.h"

// 5. Project driver headers
#include "board/drivers/uart.h"
#include "board/drivers/can_common.h"

// 6. External dependencies
#include "opendbc/safety/safety.h"

// 7. Application headers
#include "board/health.h"
#include "board/main_comms.h"
```

#### 3.2 Conditional Includes
```c
// Platform-specific includes
#ifdef STM32H7
  #include "board/stm32h7/stm32h7_config.h"
#else
  #include "fake_stm.h"
#endif

// Debug includes
#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
  #include "board/drivers/debug_uart.h"
#endif
```

### 4. Build System Integration

#### 4.1 SCons Integration
The build system automatically compiles all `.c` files referenced from main.c:

```python
# From SConscript - no changes needed for refactoring
main_elf = env.Program(f"{project_dir}/main.elf", [
  startup,
  main    # main.c will include all necessary headers
], LINKFLAGS=[f"-Wl,--section-start,.isr_vector={project['APP_START_ADDRESS']}"] + flags)
```

#### 4.2 Compilation Database
```python
# Automatic compilation database generation
env = Environment(
  COMPILATIONDB_USE_ABSPATH=True,
  tools=["default", "compilation_db"],
)
env.CompilationDatabase("compile_commands.json")
```

---

## SECURITY STANDARDS

### 1. Input Validation

#### 1.1 Parameter Validation
```c
// Always validate parameters
bool can_push(can_ring *q, const CANPacket_t *elem) {
  if ((q == NULL) || (elem == NULL)) {
    return false;
  }
  
  // Bounds checking
  if (((q->w_ptr + 1U) % q->fifo_size) == q->r_ptr) {
    return false;  // Queue full
  }
  
  // Safe operation
  q->elems[q->w_ptr] = *elem;
  q->w_ptr = (q->w_ptr + 1U) % q->fifo_size;
  return true;
}
```

#### 1.2 Buffer Overflow Prevention
```c
// Use safe buffer patterns
#define USBPACKET_MAX_SIZE 0x40U
#define MAX_CAN_MSGS_PER_USB_BULK_TRANSFER 51U

// Bounds checking in loops
for (i = 0; (i < len) && (i < MAX_BUFFER_SIZE); i++) {
  buffer[i] = data[i];
}

// Array size validation
COMPILE_TIME_ASSERT(sizeof(buffer) >= MIN_REQUIRED_SIZE);
```

### 2. Safe Function Usage

#### 2.1 Preferred Safe Functions
```c
// Use bounded operations
memcpy(dest, src, MIN(sizeof(dest), len));

// Avoid unbounded operations
// strcpy() - NEVER use
// strcat() - NEVER use  
// sprintf() - Use with caution, prefer snprintf()

// Use helper macros for safety
#define CLAMP(x, low, high) ({ \
  __typeof__(x) __x = (x); \
  __typeof__(low) __low = (low);\
  __typeof__(high) __high = (high);\
  (__x > __high) ? __high : ((__x < __low) ? __low : __x); \
})
```

#### 2.2 Integer Overflow Protection
```c
// Safe arithmetic with overflow checking
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  // Overflow is properly handled by uint32_t wraparound
  return ts - ts_last;
}

// Explicit bounds checking for critical operations
if ((value > 0U) && (multiplier > (UINT32_MAX / value))) {
  // Handle overflow condition
  return UINT32_MAX;
}
```

### 3. Volatile and Atomic Operations

#### 3.1 Volatile Variables
```c
// Hardware registers and interrupt-shared data
typedef struct {
  volatile uint32_t w_ptr;  // Modified by interrupts
  volatile uint32_t r_ptr;  // Modified by interrupts
  uint32_t fifo_size;       // Read-only after init
  CANPacket_t *elems;       // Pointer to buffer
} can_ring;
```

#### 3.2 Critical Sections
```c
// Interrupt disable patterns
static volatile bool interrupts_enabled = false;

#define ENTER_CRITICAL() do { \
  __disable_irq(); \
  interrupts_enabled = false; \
} while(0)

#define EXIT_CRITICAL() do { \
  interrupts_enabled = true; \
  __enable_irq(); \
} while(0)
```

---

## QUALITY ASSURANCE

### 1. Code Review Requirements

#### 1.1 Review Checklist
- [ ] **MISRA C compliance**: No new violations introduced
- [ ] **Memory safety**: All buffer accesses bounds-checked
- [ ] **Function separation**: Implementation moved to .c file
- [ ] **Header structure**: Follows standard organization
- [ ] **Include minimization**: Only necessary headers included
- [ ] **Documentation**: Complex functions documented
- [ ] **Testing**: Unit tests provided for new .c files

#### 1.2 Critical Review Areas
- **Safety functions**: Extra scrutiny for CAN, safety mode, power management
- **Interrupt handlers**: Verify thread safety and timing constraints
- **Buffer operations**: Check for overflows, underflows, race conditions
- **Error handling**: Ensure all error paths are handled gracefully

### 2. Build Testing Procedures

#### 2.1 Standard Build Test
```bash
# Full build test
cd /home/dholzric/projects/comma/openpilot/panda
scons -j$(nproc)

# MISRA compliance check  
./tests/misra/test_misra.sh

# Unit tests (if available)
scons --extras
```

#### 2.2 Multi-Configuration Testing
```bash
# Debug build
DEBUG=1 scons

# Release build  
RELEASE=1 CERT=/path/to/cert scons

# Jungle variant
PANDA_JUNGLE=1 scons
```

### 3. Static Analysis Requirements

#### 3.1 Cppcheck Integration
```bash
# Standard MISRA check from test_misra.sh
cppcheck --enable=all --disable=unusedFunction --addon=misra \
  -DSTM32H7 -DSTM32H725xx -I board/stm32h7/inc/ \
  board/main.c
```

#### 3.2 Additional Static Analysis
```bash
# GCC static analysis
gcc -fanalyzer -Wall -Wextra -Werror

# Clang static analyzer
clang --analyze
```

### 4. Documentation Requirements

#### 4.1 Function Documentation
```c
/**
 * Calculate CRC checksum using specified polynomial
 * 
 * @param dat Pointer to data buffer
 * @param len Length of data in bytes  
 * @param poly CRC polynomial
 * @return Calculated CRC value
 * 
 * @note Processing is done in reverse byte order
 * @warning dat must point to at least len bytes
 */
uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly);
```

#### 4.2 Module Documentation
```c
/**
 * @file crc.h
 * @brief CRC calculation utilities
 * 
 * Provides CRC checksum calculation functions for data integrity
 * verification in automotive communication protocols.
 * 
 * @author comma.ai
 * @date 2025-10-20
 * @version 1.0
 */
```

---

## EXAMPLES AND PATTERNS

### 1. Complete Refactoring Example

#### 1.1 Before: utils.h (implementation in header)
```c
// BEFORE - BAD PATTERN
#pragma once

// Macros are OK in headers
#define MIN(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a < _b) ? _a : _b; \
})

// PROBLEM: Implementation in header
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  return ts - ts_last;
}
```

#### 1.2 After: Proper Separation

**utils.h (declarations only):**
```c
#pragma once

#include <stdint.h>

// ***************************** Macro Definitions *****************************
#define MIN(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a < _b) ? _a : _b; \
})

#define MAX(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a > _b) ? _a : _b; \
})

#define CLAMP(x, low, high) ({ \
  __typeof__(x) __x = (x); \
  __typeof__(low) __low = (low);\
  __typeof__(high) __high = (high);\
  (__x > __high) ? __high : ((__x < __low) ? __low : __x); \
})

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * (!(pred) ? 1 : 0))]))

// ***************************** Function Prototypes *****************************
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last);
```

**utils.c (implementations):**
```c
#include "board/utils.h"

/**
 * Compute the time elapsed (in microseconds) from 2 counter samples
 * Case where ts < ts_last is ok: overflow is properly re-casted into uint32_t
 */
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  return ts - ts_last;
}
```

### 2. Driver Refactoring Pattern

#### 2.1 CAN Driver Example

**can_common_declarations.h:**
```c
#pragma once

#include "board/can.h"

// ***************************** Type Definitions *****************************
typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

typedef struct {
  uint8_t bus_lookup;
  uint8_t can_num_lookup;
  int8_t forwarding_bus;
  uint32_t can_speed;
  uint32_t can_data_speed;
  bool canfd_auto;
  bool canfd_enabled;
  bool brs_enabled;
  bool canfd_non_iso;
} bus_config_t;

// ***************************** Global Variables *****************************
extern uint32_t safety_tx_blocked;
extern uint32_t safety_rx_invalid;
extern can_health_t can_health[PANDA_CAN_CNT];
extern bool can_silent;
extern can_ring *can_queues[PANDA_CAN_CNT];
extern bus_config_t bus_config[PANDA_CAN_CNT];

// ***************************** Function Prototypes *****************************
bool can_init(uint8_t can_number);
void process_can(uint8_t can_number);
bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, const CANPacket_t *elem);
uint32_t can_slots_empty(const can_ring *q);
void can_init_all(void);
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool can_tx_check_min_slots_free(uint32_t min);
uint8_t calculate_checksum(const uint8_t *dat, uint32_t len);

// ***************************** Helper Macros *****************************
#define CANIF_FROM_CAN_NUM(num) (cans[num])
#define BUS_NUM_FROM_CAN_NUM(num) (bus_config[num].bus_lookup)
#define CAN_NUM_FROM_BUS_NUM(num) (bus_config[num].can_num_lookup)

#define WORD_TO_BYTE_ARRAY(dst8, src32) \
  0[dst8] = ((src32) & 0xFFU); \
  1[dst8] = (((src32) >> 8U) & 0xFFU); \
  2[dst8] = (((src32) >> 16U) & 0xFFU); \
  3[dst8] = (((src32) >> 24U) & 0xFFU)

#define BYTE_ARRAY_TO_WORD(dst32, src8) \
  ((dst32) = 0[src8] | (1[src8] << 8U) | (2[src8] << 16U) | (3[src8] << 24U))
```

**can_common.c:**
```c
#include "board/config.h"
#include "board/drivers/can_common_declarations.h"
#include "board/critical.h"

// Implementation of all functions declared in can_common_declarations.h
bool can_push(can_ring *q, const CANPacket_t *elem) {
  if ((q == NULL) || (elem == NULL)) {
    return false;
  }
  
  bool ret = false;
  ENTER_CRITICAL();
  if (((q->w_ptr + 1U) % q->fifo_size) != q->r_ptr) {
    q->elems[q->w_ptr] = *elem;
    q->w_ptr = (q->w_ptr + 1U) % q->fifo_size;
    ret = true;
  }
  EXIT_CRITICAL();
  return ret;
}

// ... other implementations
```

### 3. Error Handling Patterns

#### 3.1 Robust Error Handling
```c
// Function with comprehensive error handling
bool can_init(uint8_t can_number) {
  // Parameter validation
  if (can_number >= PANDA_CAN_CNT) {
    return false;
  }
  
  // Resource allocation
  can_ring *q = can_queues[can_number];
  if (q == NULL) {
    return false;
  }
  
  // Hardware initialization with timeout
  uint32_t timeout = CAN_INIT_TIMEOUT_MS;
  while ((timeout > 0U) && !hardware_ready()) {
    timeout--;
    delay_ms(1U);
  }
  
  if (timeout == 0U) {
    // Cleanup on failure
    can_queues[can_number] = NULL;
    return false;
  }
  
  // Success path
  q->w_ptr = 0U;
  q->r_ptr = 0U;
  return true;
}
```

#### 3.2 Safety-Critical Error Handling
```c
// Terminal error handling for safety systems
void set_safety_mode(uint16_t mode, uint16_t param) {
  uint16_t mode_copy = mode;
  int err = set_safety_hooks(mode_copy, param);
  
  if (err == -1) {
    print("Error: safety set mode failed. Falling back to SILENT\n");
    mode_copy = SAFETY_SILENT;
    err = set_safety_hooks(mode_copy, 0U);
    
    // TERMINAL ERROR: we can't continue if SILENT safety mode isn't successfully set
    assert_fatal(err == 0, "Error: Failed setting SILENT mode. Hanging\n");
  }
  
  // Configure system based on successful mode setting
  switch (mode_copy) {
    case SAFETY_SILENT:
      can_silent = true;
      break;
    default:
      can_silent = false;
      break;
  }
}
```

---

## TESTING REQUIREMENTS

### 1. Unit Testing Standards

#### 1.1 Test Structure for New .c Files
```c
// test_crc.c - Unit tests for crc.c
#include "board/crc.h"
#include "test_framework.h"

void test_crc_checksum_known_values(void) {
  // Test with known good values
  const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t result = crc_checksum(test_data, 4, 0x1D);
  
  TEST_ASSERT_EQUAL_HEX8(0x5A, result);  // Known good result
}

void test_crc_checksum_empty_data(void) {
  // Edge case: empty data
  uint8_t result = crc_checksum(NULL, 0, 0x1D);
  TEST_ASSERT_EQUAL_HEX8(0xFF, result);  // Should return initial value
}

void test_crc_checksum_single_byte(void) {
  // Edge case: single byte
  const uint8_t test_data[] = {0xFF};
  uint8_t result = crc_checksum(test_data, 1, 0x1D);
  
  // Verify against manual calculation
  TEST_ASSERT_EQUAL_HEX8(0x1A, result);
}

int main(void) {
  test_crc_checksum_known_values();
  test_crc_checksum_empty_data();
  test_crc_checksum_single_byte();
  
  print("CRC tests passed\n");
  return 0;
}
```

#### 1.2 Integration Testing
```bash
# Build and test after each refactoring
cd /home/dholzric/projects/comma/openpilot/panda

# 1. Clean build
scons -c
scons

# 2. MISRA compliance
./tests/misra/test_misra.sh

# 3. Hardware-in-the-loop tests
python tests/hitl/1_program.py
python tests/hitl/2_health.py
python tests/hitl/4_can_loopback.py

# 4. Static analysis
cppcheck --enable=all --addon=misra board/main.c
```

### 2. Regression Testing

#### 2.1 Functional Tests
- **CAN communication**: Verify message transmission/reception
- **Safety systems**: Test all safety mode transitions
- **Power management**: Verify sleep/wake cycles
- **USB communication**: Test host communication protocols

#### 2.2 Performance Tests  
- **Real-time constraints**: Verify interrupt response times
- **Memory usage**: Check stack and heap consumption
- **Boot time**: Ensure fast startup requirements met

### 3. Test Coverage Requirements

#### 3.1 Code Coverage Targets
- **Function coverage**: 100% - all functions must be called
- **Branch coverage**: 95% - all decision points tested
- **Line coverage**: 90% - most code lines executed

#### 3.2 Safety-Critical Coverage
- **Safety functions**: 100% coverage mandatory
- **Error handling**: All error paths tested
- **Edge cases**: Boundary conditions verified

---

## CONCLUSION

This coding standards document provides the foundation for safe, maintainable, and professional automotive firmware development for Panda Bounty #2171. The standards ensure:

1. **MISRA C compliance** for automotive safety
2. **Proper separation** of interface and implementation
3. **Consistent patterns** with existing panda codebase
4. **Security best practices** for embedded systems
5. **Comprehensive testing** requirements

All refactoring work must adhere to these standards to ensure the resulting firmware maintains the high safety and quality standards required for automotive applications.

**Key Success Metrics:**
- Zero new MISRA C violations
- All builds pass without warnings
- Hardware-in-the-loop tests continue to pass
- Code maintainability improved through proper separation
- Security vulnerabilities eliminated

For questions or clarifications on these standards, refer to the existing panda codebase patterns and the MISRA C 2012 specification.