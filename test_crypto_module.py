#!/usr/bin/env python3
"""
Test script to verify the crypto module compiles in isolation.

This script tests that the crypto module can be built independently
without any dependencies on the rest of the panda build system.
"""

import os
import sys
import tempfile
import subprocess

def test_crypto_module_isolation():
    """Test that crypto module builds in complete isolation."""

    print("Testing crypto module in isolation...")

    # Create a temporary directory for isolated build
    with tempfile.TemporaryDirectory() as temp_dir:
        print(f"Using temporary directory: {temp_dir}")

        # Copy only the crypto module files
        crypto_src_dir = "/home/dholzric/projects/comma/openpilot/panda/modules/crypto"
        test_dir = os.path.join(temp_dir, "crypto_test")
        os.makedirs(test_dir)

        # Copy source files
        import shutil
        for filename in ['rsa.c', 'sha.c', 'rsa.h', 'sha.h', 'hash-internal.h']:
            src_path = os.path.join(crypto_src_dir, filename)
            dst_path = os.path.join(test_dir, filename)
            shutil.copy2(src_path, dst_path)
            print(f"Copied {filename}")

        # Create a simple test program
        test_program = """
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rsa.h"
#include "sha.h"

int main() {
    printf("Testing crypto module isolation...\\n");

    // Test SHA-1 basic functionality
    uint8_t test_data[] = "hello world";
    uint8_t digest[SHA_DIGEST_SIZE];

    SHA_hash(test_data, strlen((char*)test_data), digest);

    printf("SHA-1 test: ");
    for (int i = 0; i < SHA_DIGEST_SIZE; i++) {
        printf("%02x", digest[i]);
    }
    printf("\\n");

    // Test RSA structure
    RSAPublicKey key = {0};
    key.len = RSANUMWORDS;
    key.exponent = 65537;

    printf("RSA key length: %d words\\n", key.len);
    printf("RSA exponent: %d\\n", key.exponent);

    printf("Crypto module isolation test PASSED\\n");
    return 0;
}
"""

        test_file = os.path.join(test_dir, "test_crypto.c")
        with open(test_file, 'w') as f:
            f.write(test_program)

        print("Created test program")

        # Try to compile with minimal flags
        compile_cmd = [
            'gcc',
            '-std=c99',
            '-Wall', '-Wextra',
            '-DCRYPTO_HAS_MEMCPY',  # Use system memcpy
            '-I.',
            'test_crypto.c', 'rsa.c', 'sha.c',
            '-o', 'test_crypto'
        ]

        print(f"Compiling with: {' '.join(compile_cmd)}")

        # Run compilation
        try:
            result = subprocess.run(
                compile_cmd,
                cwd=test_dir,
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                print("✓ Compilation successful!")

                # Try to run the test
                run_result = subprocess.run(
                    ['./test_crypto'],
                    cwd=test_dir,
                    capture_output=True,
                    text=True,
                    timeout=10
                )

                if run_result.returncode == 0:
                    print("✓ Test execution successful!")
                    print("Test output:")
                    print(run_result.stdout)
                    return True
                else:
                    print("✗ Test execution failed:")
                    print(run_result.stderr)
                    return False
            else:
                print("✗ Compilation failed:")
                print("STDOUT:", result.stdout)
                print("STDERR:", result.stderr)
                return False

        except subprocess.TimeoutExpired:
            print("✗ Compilation timeout")
            return False
        except Exception as e:
            print(f"✗ Compilation error: {e}")
            return False

def test_header_compilation():
    """Test that each header compiles independently."""

    print("\nTesting header compilation independence...")

    crypto_src_dir = "/home/dholzric/projects/comma/openpilot/panda/modules/crypto"
    headers = ['rsa.h', 'sha.h', 'hash-internal.h']

    for header in headers:
        print(f"Testing {header}...")

        with tempfile.TemporaryDirectory() as temp_dir:
            # Copy header and dependencies
            import shutil
            header_path = os.path.join(crypto_src_dir, header)
            test_header_path = os.path.join(temp_dir, header)
            shutil.copy2(header_path, test_header_path)

            # For sha.h, also copy hash-internal.h since it's included
            if header == 'sha.h':
                dep_path = os.path.join(crypto_src_dir, 'hash-internal.h')
                test_dep_path = os.path.join(temp_dir, 'hash-internal.h')
                shutil.copy2(dep_path, test_dep_path)

            # Create test file
            test_content = f"""
#include <stdint.h>
#include "{header}"

int main(void) {{
    return 0;
}}
"""
            test_file = os.path.join(temp_dir, 'test.c')
            with open(test_file, 'w') as f:
                f.write(test_content)

            # Compile
            compile_cmd = ['gcc', '-std=c99', '-c', 'test.c', '-I.']

            try:
                result = subprocess.run(
                    compile_cmd,
                    cwd=temp_dir,
                    capture_output=True,
                    text=True,
                    timeout=10
                )

                if result.returncode == 0:
                    print(f"  ✓ {header} compiles independently")
                else:
                    print(f"  ✗ {header} failed to compile:")
                    print(f"    {result.stderr}")
                    return False

            except Exception as e:
                print(f"  ✗ {header} compilation error: {e}")
                return False

    return True

if __name__ == "__main__":
    print("Crypto Module Isolation Test")
    print("=" * 40)

    success = True

    # Test module isolation
    success &= test_crypto_module_isolation()

    # Test header independence
    success &= test_header_compilation()

    print("\n" + "=" * 40)
    if success:
        print("✓ All crypto module isolation tests PASSED")
        sys.exit(0)
    else:
        print("✗ Some crypto module isolation tests FAILED")
        sys.exit(1)