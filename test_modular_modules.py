#!/usr/bin/env python3
"""
Test script for validating the new modular modules
"""

import os
import sys

# Add modules directory to Python path
sys.path.insert(0, 'modules')
from module_registry import module_registry

def test_module_loading():
    """Test that all new modules can be registered properly."""
    print("Testing module registration and dependencies...")
    
    try:
        # Test HAL STM32H7 module (foundation layer)
        hal_module = module_registry.register_module(
            name='hal_stm32h7_test',
            description='Test HAL module',
            sources=['startup_stm32h7x5xx.s'],
            headers=['board.h', 'clock.h'],
            includes=['.', 'inc'],
            dependencies=[],
            directory='modules/hal_stm32h7'
        )
        print("✓ HAL STM32H7 module registration successful")
        
        # Test drivers_basic module (depends on HAL)
        drivers_basic_module = module_registry.register_module(
            name='drivers_basic_test',
            description='Test drivers basic module',
            sources=[],
            headers=['gpio.h', 'led.h'],
            includes=['.'],
            dependencies=['hal_stm32h7_test'],
            directory='modules/drivers_basic'
        )
        print("✓ Drivers basic module registration successful")
        
        # Test drivers_comm module (depends on drivers_basic)
        drivers_comm_module = module_registry.register_module(
            name='drivers_comm_test',
            description='Test drivers comm module',
            sources=[],
            headers=['uart.h', 'usb.h'],
            includes=['.'],
            dependencies=['drivers_basic_test'],
            directory='modules/drivers_comm'
        )
        print("✓ Drivers comm module registration successful")
        
        # Test dependency resolution
        print("\nTesting dependency resolution...")
        build_order = module_registry.get_build_order('drivers_comm_test')
        expected_order = ['hal_stm32h7_test', 'drivers_basic_test', 'drivers_comm_test']
        
        if build_order == expected_order:
            print(f"✓ Build order correct: {' -> '.join(build_order)}")
        else:
            print(f"✗ Build order incorrect. Expected: {expected_order}, Got: {build_order}")
            return False
            
        # Test dependency validation
        module_registry.validate_all_dependencies()
        print("✓ Dependency validation successful")
        
        # Test include path resolution
        includes = module_registry.get_all_includes('drivers_comm_test')
        print(f"✓ Include path resolution: {includes}")
        
        # Test module statistics
        stats = module_registry.get_stats()
        print(f"✓ Module statistics: {stats}")
        
        return True
        
    except Exception as e:
        print(f"✗ Module testing failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_file_structure():
    """Test that all module files exist."""
    print("\nTesting module file structure...")
    
    modules_to_check = [
        ('hal_stm32h7', [
            'SConscript', 'README.md', 'board.h', 'clock.h', 
            'startup_stm32h7x5xx.s', 'inc/stm32h7xx.h'
        ]),
        ('drivers_basic', [
            'SConscript', 'README.md', 'gpio.h', 'led.h', 
            'timers.h', 'pwm.h'
        ]),
        ('drivers_comm', [
            'SConscript', 'README.md', 'uart.h', 'usb.h', 'spi.h'
        ]),
        ('crypto', [
            'SConscript', 'README.md', 'rsa.h', 'sha.h', 'rsa.c', 'sha.c'
        ])
    ]
    
    all_files_exist = True
    
    for module_name, files in modules_to_check:
        module_dir = f'modules/{module_name}'
        if not os.path.exists(module_dir):
            print(f"✗ Module directory missing: {module_dir}")
            all_files_exist = False
            continue
            
        print(f"Checking {module_name} module files...")
        for file_name in files:
            file_path = os.path.join(module_dir, file_name)
            if os.path.exists(file_path):
                print(f"  ✓ {file_name}")
            else:
                print(f"  ✗ {file_name} (missing)")
                all_files_exist = False
    
    return all_files_exist

def main():
    """Run all tests."""
    print("=" * 60)
    print("MODULAR BUILD SYSTEM VALIDATION")
    print("=" * 60)
    
    # Test file structure
    files_ok = test_file_structure()
    
    # Test module loading
    modules_ok = test_module_loading()
    
    print("\n" + "=" * 60)
    if files_ok and modules_ok:
        print("✓ ALL TESTS PASSED - Modular build system is ready!")
        print("\nNext steps:")
        print("1. Run: scons -f SConscript.incremental")
        print("2. Check build output for module loading messages")
        print("3. Verify incremental targets are created")
        return 0
    else:
        print("✗ TESTS FAILED - Issues found in modular build system")
        if not files_ok:
            print("  - File structure issues detected")
        if not modules_ok:
            print("  - Module registration/dependency issues detected")
        return 1

if __name__ == '__main__':
    sys.exit(main())