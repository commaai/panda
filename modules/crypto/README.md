# Crypto Module

## Overview

The crypto module provides cryptographic functions for secure boot and firmware signature verification in the panda project. This module is completely self-contained with no dependencies on other panda modules, making it an ideal example of the modular build system.

## Features

- **RSA Signature Verification**: 1024-bit RSA signature verification with support for both e=3 and e=65537 exponents
- **SHA-1 Hashing**: Complete SHA-1 implementation with incremental and one-shot hashing
- **Secure Boot Support**: Validates firmware signatures during the boot process
- **Self-Contained**: No external dependencies beyond standard C library

## Architecture

```
crypto/
├── README.md              # This documentation
├── SConscript            # Comprehensive build script
├── rsa.h                 # RSA public interface
├── rsa.c                 # RSA implementation  
├── sha.h                 # SHA-1 public interface
├── sha.c                 # SHA-1 implementation
├── hash-internal.h       # Internal hash context definitions
└── sign.py              # Firmware signing utility
```

## Public Interface

### RSA Functions

```c
// Verify RSA signature against a hash
int RSA_verify(const RSAPublicKey *key,
               const uint8_t *signature,
               const int len,
               const uint8_t *hash,
               const int hash_len);
```

### SHA-1 Functions

```c
// One-shot hash function
const uint8_t* SHA_hash(const void* data, int len, uint8_t* digest);

// Incremental hashing
void SHA_init(SHA_CTX* ctx);
void SHA_update(SHA_CTX* ctx, const void* data, int len);
const uint8_t* SHA_final(SHA_CTX* ctx);
```

### Key Types

```c
typedef struct RSAPublicKey {
    int len;                  // Length of n[] in number of uint32_t
    uint32_t n0inv;           // -1 / n[0] mod 2^32
    uint32_t n[RSANUMWORDS];  // modulus as little endian array
    uint32_t rr[RSANUMWORDS]; // R^2 as little endian array
    int exponent;             // 3 or 65537
} RSAPublicKey;

typedef HASH_CTX SHA_CTX;    // SHA-1 context
```

## Constants

- `RSANUMBYTES`: 128 (1024-bit key length in bytes)
- `RSANUMWORDS`: 32 (1024-bit key length in 32-bit words)
- `SHA_DIGEST_SIZE`: 20 (SHA-1 digest size in bytes)

## Usage Examples

### Basic RSA Verification

```c
#include "crypto/rsa.h"
#include "crypto/sha.h"

// Verify firmware signature (typical bootstub usage)
uint8_t digest[SHA_DIGEST_SIZE];
SHA_hash(firmware_data, firmware_len, digest);

if (RSA_verify(&release_key, signature, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    // Signature valid - boot firmware
} else {
    // Signature invalid - boot failed
}
```

### Incremental Hashing

```c
#include "crypto/sha.h"

SHA_CTX ctx;
uint8_t digest[SHA_DIGEST_SIZE];

SHA_init(&ctx);
SHA_update(&ctx, data_chunk1, len1);
SHA_update(&ctx, data_chunk2, len2);
SHA_final(&ctx);

// digest is now available in ctx.buf
```

## Integration

### Bootstub Integration

The crypto module is primarily used in `board/bootstub.c` for secure boot:

```c
#include "crypto/rsa.h"
#include "crypto/sha.h"

// Compute firmware hash
uint8_t digest[SHA_DIGEST_SIZE];
SHA_hash(&_app_start[1], len-4, digest);

// Verify with release key
if (RSA_verify(&release_rsa_key, signature_ptr, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
}

// Verify with debug key (if allowed)
#ifdef ALLOW_DEBUG
if (RSA_verify(&debug_rsa_key, signature_ptr, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
}
#endif
```

### Build Integration

The module integrates with the modular build system:

```python
# In SConscript
Import('env', 'module_registry')

# Load crypto module
crypto_module = SConscript('modules/crypto/SConscript', 
                          exports=['env', 'module_registry'])

# Use crypto objects in builds
crypto_objects = module_registry.get_module('crypto').built_objects
```

## Security Considerations

- **Key Management**: RSA keys are embedded as compile-time constants
- **Timing Attacks**: Implementation uses constant-time operations where possible
- **Memory Protection**: Stack protection and buffer overflow detection enabled
- **Signature Validation**: Strict PKCS#1.5 padding validation
- **Hash Verification**: Complete hash comparison prevents partial matches

## Testing

### Unit Tests

The module includes comprehensive test coverage:

- `test_rsa_basic.c`: Basic RSA verification tests
- `test_sha_vectors.c`: SHA-1 test vectors from RFC 3174
- `test_crypto_integration.c`: Integration tests with bootstub

### Header Validation

Build system validates that all headers compile independently:

```bash
scons --validate_modules
```

### Performance Benchmarks

Performance testing ensures crypto operations meet timing requirements:

- RSA verification: < 50ms on STM32H7
- SHA-1 hashing: > 1MB/s throughput

## Dependencies

### Internal Dependencies
- None (completely self-contained)

### External Dependencies
- `stdint.h`: Standard integer types
- Standard C library functions (memcpy)

### Build Dependencies
- SCons build system
- ARM GCC toolchain
- Module registry system

## Compliance

- **Coding Standards**: MISRA C compliance where applicable
- **License**: BSD-3-Clause (Android Open Source Project)
- **Cryptographic Standards**: 
  - RSA PKCS#1.5 signature verification
  - SHA-1 hashing per FIPS 180-1

## Migration Guide

This module serves as a template for converting other modules to the modular system:

1. **Copy Structure**: Use this module's directory structure as template
2. **Adapt SConscript**: Modify the comprehensive build script
3. **Update Dependencies**: Declare all module dependencies explicitly
4. **Add Tests**: Include unit tests and validation
5. **Document Interface**: Provide complete API documentation

## Future Enhancements

Planned improvements to maintain the module as a reference implementation:

- SHA-256 support for larger hash sizes
- RSA-2048 support for stronger signatures  
- Hardware acceleration on STM32H7 crypto peripherals
- Additional test vectors and benchmarks
- MISRA C compliance certification

## Support

For questions about the crypto module or modular build system:

- Review existing validation reports in `/board/PANDA_BOUNTY_*`
- Check module registry documentation in `/modules/module_registry.py`
- Examine incremental build examples in `/SConscript.incremental`