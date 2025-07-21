#ifndef STDINT_H_INCLUDED
#define STDINT_H_INCLUDED

// Basic integer types for embedded ARM
typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

// 64-bit types needed by crypto and CMSIS functions
typedef long long int int64_t;
typedef unsigned long long int uint64_t;

// These typedefs are used in the project
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;

// Limits
#define INT8_MIN (-128)
#define INT8_MAX (127)
#define UINT8_MAX (255)

#define INT16_MIN (-32768)
#define INT16_MAX (32767)
#define UINT16_MAX (65535)

#define INT32_MIN (-2147483648)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295U)

#define INT64_MIN (-9223372036854775808LL)
#define INT64_MAX (9223372036854775807LL)
#define UINT64_MAX (18446744073709551615ULL)

#define PANDA_INTPTR_MIN INT32_MIN
#define PANDA_INTPTR_MAX INT32_MAX
#define PANDA_UINTPTR_MAX UINT32_MAX

#define PANDA_INTMAX_MIN INT64_MIN
#define PANDA_INTMAX_MAX INT64_MAX
#define PANDA_UINTMAX_MAX UINT64_MAX

#define PANDA_SIZE_MAX UINT32_MAX

#endif /* STDINT_H_INCLUDED */