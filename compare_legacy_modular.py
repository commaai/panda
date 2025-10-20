#!/usr/bin/env python3
"""
Legacy vs Modular Build Comparison

This script builds both the legacy and modular versions of the crypto module
and compares their outputs to ensure equivalence.
"""

import os
import sys
import subprocess
import tempfile
import hashlib
import time
import json
from pathlib import Path

class LegacyModularComparator:
    """Compare legacy and modular crypto implementations."""
    
    def __init__(self, panda_root):
        self.panda_root = Path(panda_root)
        self.results = {
            'timestamp': time.time(),
            'legacy': {},
            'modular': {},
            'comparison': {},
            'summary': {'equivalent': True, 'differences': []}
        }
    
    def build_legacy_crypto(self):
        """Build crypto module using legacy approach."""
        print("Building legacy crypto module...")
        
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            
            # Copy legacy crypto files
            legacy_crypto_dir = self.panda_root / 'crypto'
            import shutil
            
            for filename in ['rsa.c', 'sha.c', 'rsa.h', 'sha.h', 'hash-internal.h']:
                src_path = legacy_crypto_dir / filename
                dst_path = temp_path / filename
                if src_path.exists():
                    shutil.copy2(src_path, dst_path)
            
            # Create test program for legacy
            test_content = '''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Legacy approach - include crypto headers directly
#include "rsa.h"
#include "sha.h"

int test_sha1() {
    uint8_t test_data[] = "test_vector_123";
    uint8_t digest[SHA_DIGEST_SIZE];
    
    SHA_hash(test_data, strlen((char*)test_data), digest);
    
    printf("SHA1:");
    for (int i = 0; i < SHA_DIGEST_SIZE; i++) {
        printf("%02x", digest[i]);
    }
    printf("\\n");
    
    return 0;
}

int test_rsa_structure() {
    RSAPublicKey key = {0};
    key.len = RSANUMWORDS;
    key.exponent = 65537;
    
    printf("RSA_LEN:%d\\n", key.len);
    printf("RSA_EXP:%d\\n", key.exponent);
    printf("RSA_SIZE:%zu\\n", sizeof(RSAPublicKey));
    
    return 0;
}

int main() {
    printf("LEGACY_BUILD\\n");
    test_sha1();
    test_rsa_structure();
    printf("LEGACY_COMPLETE\\n");
    return 0;
}
'''
            
            test_file = temp_path / 'legacy_test.c'
            test_file.write_text(test_content)
            
            # Compile legacy version
            compile_cmd = [
                'gcc', '-std=c99', '-Wall', '-Wextra',
                '-I.', 'legacy_test.c', 'sha.c', '-o', 'legacy_test'
            ]
            
            try:
                result = subprocess.run(
                    compile_cmd,
                    cwd=temp_path,
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                
                if result.returncode == 0:
                    # Run legacy test
                    run_result = subprocess.run(
                        ['./legacy_test'],
                        cwd=temp_path,
                        capture_output=True,
                        text=True,
                        timeout=10
                    )
                    
                    self.results['legacy'] = {
                        'build_success': True,
                        'run_success': run_result.returncode == 0,
                        'output': run_result.stdout,
                        'errors': run_result.stderr
                    }
                    
                    print("✓ Legacy build successful")
                    return True
                else:
                    self.results['legacy'] = {
                        'build_success': False,
                        'build_errors': result.stderr,
                        'run_success': False
                    }
                    print("✗ Legacy build failed")
                    return False
                    
            except Exception as e:
                self.results['legacy'] = {
                    'build_success': False,
                    'build_errors': str(e),
                    'run_success': False
                }
                print(f"✗ Legacy build exception: {e}")
                return False
    
    def build_modular_crypto(self):
        """Build crypto module using modular approach."""
        print("Building modular crypto module...")
        
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            
            # Copy modular crypto files
            modular_crypto_dir = self.panda_root / 'modules' / 'crypto'
            import shutil
            
            for filename in ['rsa.c', 'sha.c', 'rsa.h', 'sha.h', 'hash-internal.h']:
                src_path = modular_crypto_dir / filename
                dst_path = temp_path / filename
                if src_path.exists():
                    shutil.copy2(src_path, dst_path)
            
            # Create test program for modular
            test_content = '''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Modular approach - include crypto headers with proper definitions
#include "rsa.h"
#include "sha.h"

int test_sha1() {
    uint8_t test_data[] = "test_vector_123";
    uint8_t digest[SHA_DIGEST_SIZE];
    
    SHA_hash(test_data, strlen((char*)test_data), digest);
    
    printf("SHA1:");
    for (int i = 0; i < SHA_DIGEST_SIZE; i++) {
        printf("%02x", digest[i]);
    }
    printf("\\n");
    
    return 0;
}

int test_rsa_structure() {
    RSAPublicKey key = {0};
    key.len = RSANUMWORDS;
    key.exponent = 65537;
    
    printf("RSA_LEN:%d\\n", key.len);
    printf("RSA_EXP:%d\\n", key.exponent);
    printf("RSA_SIZE:%zu\\n", sizeof(RSAPublicKey));
    
    return 0;
}

int main() {
    printf("MODULAR_BUILD\\n");
    test_sha1();
    test_rsa_structure();
    printf("MODULAR_COMPLETE\\n");
    return 0;
}
'''
            
            test_file = temp_path / 'modular_test.c'
            test_file.write_text(test_content)
            
            # Compile modular version with proper flags
            compile_cmd = [
                'gcc', '-std=c99', '-Wall', '-Wextra',
                '-DCRYPTO_HAS_MEMCPY',  # Use system memcpy
                '-DCRYPTO_MODULE_BUILD=1',  # Modular build flag
                '-I.', 'modular_test.c', 'sha.c', '-o', 'modular_test'
            ]
            
            try:
                result = subprocess.run(
                    compile_cmd,
                    cwd=temp_path,
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                
                if result.returncode == 0:
                    # Run modular test
                    run_result = subprocess.run(
                        ['./modular_test'],
                        cwd=temp_path,
                        capture_output=True,
                        text=True,
                        timeout=10
                    )
                    
                    self.results['modular'] = {
                        'build_success': True,
                        'run_success': run_result.returncode == 0,
                        'output': run_result.stdout,
                        'errors': run_result.stderr
                    }
                    
                    print("✓ Modular build successful")
                    return True
                else:
                    self.results['modular'] = {
                        'build_success': False,
                        'build_errors': result.stderr,
                        'run_success': False
                    }
                    print("✗ Modular build failed")
                    print("Error:", result.stderr)
                    return False
                    
            except Exception as e:
                self.results['modular'] = {
                    'build_success': False,
                    'build_errors': str(e),
                    'run_success': False
                }
                print(f"✗ Modular build exception: {e}")
                return False
    
    def compare_outputs(self):
        """Compare outputs from legacy and modular builds."""
        print("Comparing legacy vs modular outputs...")
        
        if not (self.results['legacy'].get('run_success') and 
                self.results['modular'].get('run_success')):
            print("✗ Cannot compare - one or both builds failed")
            self.results['summary']['equivalent'] = False
            self.results['summary']['differences'].append("Build failure prevents comparison")
            return False
        
        legacy_output = self.results['legacy']['output']
        modular_output = self.results['modular']['output']
        
        # Parse outputs
        legacy_lines = legacy_output.strip().split('\\n')
        modular_lines = modular_output.strip().split('\\n')
        
        # Extract key values for comparison
        def extract_values(lines):
            values = {}
            for line in lines:
                if line.startswith('SHA1:'):
                    values['sha1'] = line.split(':', 1)[1]
                elif line.startswith('RSA_LEN:'):
                    values['rsa_len'] = line.split(':', 1)[1]
                elif line.startswith('RSA_EXP:'):
                    values['rsa_exp'] = line.split(':', 1)[1]
                elif line.startswith('RSA_SIZE:'):
                    values['rsa_size'] = line.split(':', 1)[1]
            return values
        
        legacy_values = extract_values(legacy_lines)
        modular_values = extract_values(modular_lines)
        
        # Compare values
        equivalent = True
        differences = []
        
        for key in set(legacy_values.keys()) | set(modular_values.keys()):
            legacy_val = legacy_values.get(key, 'MISSING')
            modular_val = modular_values.get(key, 'MISSING')
            
            if legacy_val != modular_val:
                equivalent = False
                differences.append(f"{key}: legacy={legacy_val}, modular={modular_val}")
                print(f"✗ Difference in {key}: legacy={legacy_val}, modular={modular_val}")
            else:
                print(f"✓ {key} matches: {legacy_val}")
        
        self.results['comparison'] = {
            'legacy_values': legacy_values,
            'modular_values': modular_values,
            'equivalent': equivalent,
            'differences': differences
        }
        
        self.results['summary']['equivalent'] = equivalent
        self.results['summary']['differences'] = differences
        
        if equivalent:
            print("✓ Legacy and modular outputs are equivalent")
        else:
            print("✗ Legacy and modular outputs differ")
        
        return equivalent
    
    def test_file_structure_differences(self):
        """Test differences in file structure between legacy and modular."""
        print("Analyzing file structure differences...")
        
        legacy_dir = self.panda_root / 'crypto'
        modular_dir = self.panda_root / 'modules' / 'crypto'
        
        # Get file lists
        def get_files(directory):
            if not directory.exists():
                return set()
            return {f.name for f in directory.iterdir() if f.is_file()}
        
        legacy_files = get_files(legacy_dir)
        modular_files = get_files(modular_dir)
        
        # Compare
        only_legacy = legacy_files - modular_files
        only_modular = modular_files - legacy_files
        common_files = legacy_files & modular_files
        
        print(f"  Common files: {len(common_files)}")
        print(f"  Only in legacy: {len(only_legacy)}")
        print(f"  Only in modular: {len(only_modular)}")
        
        if only_legacy:
            print(f"    Legacy-only: {', '.join(only_legacy)}")
        if only_modular:
            print(f"    Modular-only: {', '.join(only_modular)}")
        
        # Check for content differences in common files
        content_differences = []
        for filename in common_files:
            if filename.endswith('.py'):  # Skip Python files for now
                continue
                
            legacy_file = legacy_dir / filename
            modular_file = modular_dir / filename
            
            try:
                legacy_content = legacy_file.read_text()
                modular_content = modular_file.read_text()
                
                if legacy_content != modular_content:
                    content_differences.append(filename)
                    print(f"    Content differs: {filename}")
                else:
                    print(f"    Content identical: {filename}")
                    
            except Exception as e:
                print(f"    Error comparing {filename}: {e}")
        
        self.results['file_structure'] = {
            'legacy_files': list(legacy_files),
            'modular_files': list(modular_files),
            'only_legacy': list(only_legacy),
            'only_modular': list(only_modular),
            'common_files': list(common_files),
            'content_differences': content_differences
        }
        
        return len(content_differences) == 0 and len(only_legacy) == 0
    
    def generate_comparison_report(self):
        """Generate comprehensive comparison report."""
        print("\\n" + "=" * 60)
        print("LEGACY VS MODULAR COMPARISON REPORT")
        print("=" * 60)
        
        # Build status
        legacy_ok = self.results['legacy'].get('build_success', False)
        modular_ok = self.results['modular'].get('build_success', False)
        
        print(f"\\nBUILD STATUS:")
        print(f"  Legacy build: {'✓ SUCCESS' if legacy_ok else '✗ FAILED'}")
        print(f"  Modular build: {'✓ SUCCESS' if modular_ok else '✗ FAILED'}")
        
        # Runtime status
        if legacy_ok and modular_ok:
            legacy_run_ok = self.results['legacy'].get('run_success', False)
            modular_run_ok = self.results['modular'].get('run_success', False)
            
            print(f"\\nRUNTIME STATUS:")
            print(f"  Legacy execution: {'✓ SUCCESS' if legacy_run_ok else '✗ FAILED'}")
            print(f"  Modular execution: {'✓ SUCCESS' if modular_run_ok else '✗ FAILED'}")
        
        # Equivalence
        equivalent = self.results['summary']['equivalent']
        print(f"\\nEQUIVALENCE:")
        print(f"  Functional equivalence: {'✓ EQUIVALENT' if equivalent else '✗ DIFFERENT'}")
        
        if not equivalent:
            differences = self.results['summary']['differences']
            print(f"  Differences found: {len(differences)}")
            for diff in differences:
                print(f"    - {diff}")
        
        # File structure
        if 'file_structure' in self.results:
            fs = self.results['file_structure']
            print(f"\\nFILE STRUCTURE:")
            print(f"  Common files: {len(fs['common_files'])}")
            print(f"  Legacy-only files: {len(fs['only_legacy'])}")
            print(f"  Modular-only files: {len(fs['only_modular'])}")
            print(f"  Content differences: {len(fs['content_differences'])}")
        
        # Save report
        report_file = self.panda_root / 'legacy_modular_comparison_report.json'
        with open(report_file, 'w') as f:
            json.dump(self.results, f, indent=2)
        
        print(f"\\nDetailed report saved to: {report_file}")
        
        return equivalent and legacy_ok and modular_ok
    
    def run_full_comparison(self):
        """Run complete comparison between legacy and modular."""
        print("Legacy vs Modular Crypto Comparison")
        print("Starting comprehensive comparison...")
        
        legacy_ok = self.build_legacy_crypto()
        modular_ok = self.build_modular_crypto()
        
        if legacy_ok and modular_ok:
            self.compare_outputs()
        
        self.test_file_structure_differences()
        
        return self.generate_comparison_report()

if __name__ == "__main__":
    comparator = LegacyModularComparator("/home/dholzric/projects/comma/openpilot/panda")
    success = comparator.run_full_comparison()
    
    sys.exit(0 if success else 1)