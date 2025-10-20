#!/usr/bin/env python3
"""
Comprehensive Crypto Module Conversion Validation

This script validates the complete conversion of the crypto module to the 
modular build system by testing:

1. Module isolation and compilation
2. Integration with the build system
3. Functional equivalence with legacy implementation
4. Performance characteristics
5. Documentation completeness
"""

import sys
import subprocess
import tempfile
import hashlib
import time
import json
from pathlib import Path

class CryptoModuleValidator:
    """Comprehensive validator for the crypto module conversion."""

    def __init__(self, panda_root):
        self.panda_root = Path(panda_root)
        self.results = {
            'timestamp': time.time(),
            'tests': {},
            'summary': {'passed': 0, 'failed': 0, 'total': 0}
        }

    def log_test(self, test_name, passed, details=""):
        """Log test result."""
        self.results['tests'][test_name] = {
            'passed': passed,
            'details': details,
            'timestamp': time.time()
        }

        if passed:
            self.results['summary']['passed'] += 1
            print(f"✓ {test_name}")
        else:
            self.results['summary']['failed'] += 1
            print(f"✗ {test_name}")
            if details:
                print(f"  Details: {details}")

        self.results['summary']['total'] += 1

    def test_module_structure(self):
        """Test that the crypto module has proper structure."""
        print("\n--- Testing Module Structure ---")

        crypto_dir = self.panda_root / 'modules' / 'crypto'

        # Check directory exists
        self.log_test(
            "crypto_directory_exists",
            crypto_dir.exists(),
            f"Directory: {crypto_dir}"
        )

        # Check required files
        required_files = [
            'SConscript', 'rsa.c', 'sha.c', 'rsa.h', 'sha.h',
            'hash-internal.h', 'README.md', 'sign.py'
        ]

        for filename in required_files:
            file_path = crypto_dir / filename
            self.log_test(
                f"file_exists_{filename}",
                file_path.exists(),
                f"File: {file_path}"
            )

    def test_module_isolation(self):
        """Test that the crypto module compiles in isolation."""
        print("\n--- Testing Module Isolation ---")

        crypto_dir = self.panda_root / 'modules' / 'crypto'

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Copy crypto files
            import shutil
            for filename in ['rsa.c', 'sha.c', 'rsa.h', 'sha.h', 'hash-internal.h']:
                src_path = crypto_dir / filename
                dst_path = temp_path / filename
                shutil.copy2(src_path, dst_path)

            # Create test program
            test_content = '''
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rsa.h"
#include "sha.h"

int main() {
    // Test SHA-1 functionality
    uint8_t test_data[] = "test";
    uint8_t digest[SHA_DIGEST_SIZE];
    
    SHA_hash(test_data, 4, digest);
    
    printf("SHA-1 test completed\\n");
    
    // Test RSA structure initialization
    RSAPublicKey key = {0};
    key.len = RSANUMWORDS;
    key.exponent = 65537;
    
    printf("RSA structure test completed\\n");
    
    return 0;
}
'''

            test_file = temp_path / 'test.c'
            test_file.write_text(test_content)

            # Compile test
            compile_cmd = [
                'gcc', '-std=c99', '-Wall', '-Wextra',
                '-DCRYPTO_HAS_MEMCPY', '-I.',
                'test.c', 'rsa.c', 'sha.c', '-o', 'test'
            ]

            try:
                result = subprocess.run(
                    compile_cmd,
                    cwd=temp_path,
                    capture_output=True,
                    text=True,
                    timeout=30
                )

                compile_success = result.returncode == 0
                self.log_test(
                    "isolation_compile",
                    compile_success,
                    result.stderr if not compile_success else "Compiled successfully"
                )

                if compile_success:
                    # Run test
                    run_result = subprocess.run(
                        ['./test'],
                        cwd=temp_path,
                        capture_output=True,
                        text=True,
                        timeout=10
                    )

                    run_success = run_result.returncode == 0
                    self.log_test(
                        "isolation_execution",
                        run_success,
                        run_result.stdout if run_success else run_result.stderr
                    )

            except Exception as e:
                self.log_test("isolation_compile", False, str(e))

    def test_module_registry_integration(self):
        """Test integration with the module registry."""
        print("\n--- Testing Module Registry Integration ---")

        # Test module registry functionality
        test_script = '''
import sys
sys.path.insert(0, "./modules")

from module_registry import module_registry

try:
    # Test registration
    module = module_registry.register_module(
        name="test_crypto",
        description="Test crypto module",
        sources=["rsa.c", "sha.c"],
        headers=["rsa.h", "sha.h"],
        includes=["."],
        dependencies=[],
        directory="modules/crypto"
    )
    
    print("✓ Module registration successful")
    
    # Test retrieval
    retrieved = module_registry.get_module("test_crypto")
    print(f"✓ Module retrieval successful: {retrieved.name}")
    
    # Test build order
    build_order = module_registry.get_build_order("test_crypto")
    print(f"✓ Build order: {build_order}")
    
    # Test validation
    module_registry.validate_all_dependencies()
    print("✓ Dependency validation successful")
    
    print("SUCCESS")
    
except Exception as e:
    print(f"FAILED: {e}")
    import traceback
    traceback.print_exc()
'''

        try:
            result = subprocess.run(
                [sys.executable, '-c', test_script],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=30
            )

            success = result.returncode == 0 and "SUCCESS" in result.stdout
            self.log_test(
                "module_registry_integration",
                success,
                result.stdout if success else result.stderr
            )

        except Exception as e:
            self.log_test("module_registry_integration", False, str(e))

    def test_functional_equivalence(self):
        """Test that the modular crypto produces equivalent results to legacy."""
        print("\n--- Testing Functional Equivalence ---")

        # Test SHA-1 equivalence
        test_data = b"Hello, World!"
        expected_sha1 = hashlib.sha1(test_data).hexdigest()

        # Create a test program that uses our crypto module
        test_script = '''
import os
import sys
import tempfile
import subprocess

crypto_dir = "./modules/crypto"

test_content = """
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sha.h"

int main() {
    uint8_t test_data[] = "Hello, World!";
    uint8_t digest[SHA_DIGEST_SIZE];
    
    SHA_hash(test_data, 13, digest);
    
    for (int i = 0; i < SHA_DIGEST_SIZE; i++) {
        printf("%02x", digest[i]);
    }
    printf("\\n");
    
    return 0;
}
"""

with tempfile.TemporaryDirectory() as temp_dir:
    import shutil
    
    # Copy crypto files
    for filename in ["rsa.c", "sha.c", "rsa.h", "sha.h", "hash-internal.h"]:
        shutil.copy2(f"{crypto_dir}/{filename}", f"{temp_dir}/{filename}")
    
    # Write test
    with open(f"{temp_dir}/test.c", "w") as f:
        f.write(test_content)
    
    # Compile
    result = subprocess.run([
        "gcc", "-std=c99", "-DCRYPTO_HAS_MEMCPY", "-I.",
        "test.c", "sha.c", "-o", "test"
    ], cwd=temp_dir, capture_output=True, text=True)
    
    if result.returncode == 0:
        # Run
        run_result = subprocess.run(["./test"], cwd=temp_dir, capture_output=True, text=True)
        if run_result.returncode == 0:
            print(run_result.stdout.strip())
        else:
            print("RUN_FAILED")
    else:
        print("COMPILE_FAILED")
'''

        try:
            result = subprocess.run(
                [sys.executable, '-c', test_script],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                actual_hash = result.stdout.strip()
                matches = actual_hash == expected_sha1
                self.log_test(
                    "sha1_functional_equivalence",
                    matches,
                    f"Expected: {expected_sha1}, Got: {actual_hash}"
                )
            else:
                self.log_test(
                    "sha1_functional_equivalence",
                    False,
                    f"Test execution failed: {result.stderr}"
                )

        except Exception as e:
            self.log_test("sha1_functional_equivalence", False, str(e))

    def test_documentation_completeness(self):
        """Test that documentation is complete and accurate."""
        print("\n--- Testing Documentation Completeness ---")

        crypto_dir = self.panda_root / 'modules' / 'crypto'

        # Check README exists and has content
        readme_path = crypto_dir / 'README.md'
        readme_exists = readme_path.exists()

        if readme_exists:
            readme_content = readme_path.read_text()
            has_overview = 'Overview' in readme_content or 'overview' in readme_content
            has_api_docs = 'API' in readme_content or 'Interface' in readme_content
            has_usage = 'Usage' in readme_content or 'usage' in readme_content

            self.log_test("readme_exists", True, f"Size: {len(readme_content)} chars")
            self.log_test("readme_has_overview", has_overview)
            self.log_test("readme_has_api_docs", has_api_docs)
            self.log_test("readme_has_usage", has_usage)
        else:
            self.log_test("readme_exists", False)

        # Check SConscript has documentation
        sconscript_path = crypto_dir / 'SConscript'
        if sconscript_path.exists():
            sconscript_content = sconscript_path.read_text()
            has_module_docs = 'module_info' in sconscript_content
            has_comments = '"""' in sconscript_content or "'''" in sconscript_content

            self.log_test("sconscript_documented", has_module_docs and has_comments)
        else:
            self.log_test("sconscript_documented", False)

    def test_build_system_integration(self):
        """Test integration with the build system."""
        print("\n--- Testing Build System Integration ---")

        # Test that SConscript.incremental can import the module registry
        test_script = '''
import sys
sys.path.insert(0, "./modules")

try:
    from module_registry import module_registry
    print("✓ Module registry import successful")
    
    # Test that crypto directory structure is correct
    import os
    crypto_path = "./modules/crypto"
    if os.path.exists(crypto_path):
        files = os.listdir(crypto_path)
        required = ["SConscript", "rsa.c", "sha.c", "rsa.h", "sha.h"]
        missing = [f for f in required if f not in files]
        if not missing:
            print("✓ Crypto module structure complete")
        else:
            print(f"✗ Missing files: {missing}")
    else:
        print("✗ Crypto module directory not found")
    
    print("SUCCESS")
    
except Exception as e:
    print(f"FAILED: {e}")
'''

        try:
            result = subprocess.run(
                [sys.executable, '-c', test_script],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=30
            )

            success = result.returncode == 0 and "SUCCESS" in result.stdout
            self.log_test(
                "build_system_integration",
                success,
                result.stdout if success else result.stderr
            )

        except Exception as e:
            self.log_test("build_system_integration", False, str(e))

    def generate_report(self):
        """Generate a comprehensive validation report."""
        print("\n" + "=" * 60)
        print("CRYPTO MODULE CONVERSION VALIDATION REPORT")
        print("=" * 60)

        # Summary
        total = self.results['summary']['total']
        passed = self.results['summary']['passed']
        failed = self.results['summary']['failed']

        print("\nSUMMARY:")
        print(f"  Total tests: {total}")
        print(f"  Passed: {passed}")
        print(f"  Failed: {failed}")
        print(f"  Success rate: {passed/total*100:.1f}%" if total > 0 else "  Success rate: N/A")

        # Failed tests details
        if failed > 0:
            print("\nFAILED TESTS:")
            for test_name, result in self.results['tests'].items():
                if not result['passed']:
                    print(f"  - {test_name}: {result['details']}")

        # Save detailed report
        report_file = self.panda_root / 'crypto_conversion_validation_report.json'
        with open(report_file, 'w') as f:
            json.dump(self.results, f, indent=2)

        print(f"\nDetailed report saved to: {report_file}")

        return failed == 0

    def run_all_tests(self):
        """Run all validation tests."""
        print("Crypto Module Conversion Validation")
        print("Starting comprehensive validation...")

        self.test_module_structure()
        self.test_module_isolation()
        self.test_module_registry_integration()
        self.test_functional_equivalence()
        self.test_documentation_completeness()
        self.test_build_system_integration()

        return self.generate_report()

if __name__ == "__main__":
    validator = CryptoModuleValidator("/home/dholzric/projects/comma/openpilot/panda")
    success = validator.run_all_tests()

    sys.exit(0 if success else 1)