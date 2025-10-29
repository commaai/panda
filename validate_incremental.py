#!/usr/bin/env python3
"""
Validation script for incremental modular refactoring.

This script validates the incremental approach by:
1. Testing the module registry system
2. Validating module definitions
3. Checking dependency resolution
4. Simulating the incremental build process
"""

import os
import sys
from pathlib import Path

# Add modules directory to path
sys.path.insert(0, 'modules')

def test_module_registry():
    """Test the module registry functionality."""
    print("Testing Module Registry...")

    from module_registry import ModuleRegistry

    # Create test registry
    registry = ModuleRegistry()

    # Test module registration
    crypto = registry.register_module(
        name='crypto',
        description='Test crypto module',
        sources=['rsa.c', 'sha.c'],
        headers=['rsa.h', 'sha.h'],
        includes=['.'],
        dependencies=[],
        directory='crypto'
    )

    registry.register_module(
        name='core',
        description='Test core module',
        sources=['core.c'],
        includes=['.'],
        dependencies=['crypto'],
        directory='board'
    )

    registry.register_module(
        name='app',
        description='Test app module',
        sources=['main.c'],
        includes=['.'],
        dependencies=['core'],
        directory='board'
    )

    print(f"✓ Registered {len(registry.modules)} modules")

    # Test dependency resolution
    build_order = registry.get_build_order('app')
    expected_order = ['crypto', 'core', 'app']

    if build_order == expected_order:
        print("✓ Dependency resolution works correctly")
    else:
        print(f"✗ Dependency resolution failed: got {build_order}, expected {expected_order}")
        return False

    # Test circular dependency detection
    try:
        crypto.dependencies.append('app')  # Create cycle
        registry.build_order_cache.clear()  # Clear cache
        registry.get_build_order('app')
        print("✗ Circular dependency detection failed")
        return False
    except ValueError as e:
        if "Circular dependency" in str(e):
            print("✓ Circular dependency detection works")
        else:
            print(f"✗ Unexpected error: {e}")
            return False

    # Test source collection
    crypto.dependencies.clear()  # Remove cycle for further testing
    registry.build_order_cache.clear()

    all_sources = registry.get_all_sources('app')
    expected_sources = ['crypto/rsa.c', 'crypto/sha.c', 'board/core.c', 'board/main.c']

    if all_sources == expected_sources:
        print("✓ Source file collection works correctly")
    else:
        print(f"✗ Source collection failed: got {all_sources}, expected {expected_sources}")
        return False

    # Test include collection
    all_includes = registry.get_all_includes('app')
    expected_includes = ['crypto/.', 'board/.']  # Relative to module directories

    # Debug print to see what we're getting
    print(f"  Debug: all_includes = {all_includes}")
    print(f"  Debug: expected_includes = {expected_includes}")

    # Check if all expected includes are present (order doesn't matter)
    if set(all_includes) >= set(expected_includes):
        print("✓ Include directory collection works correctly")
    else:
        missing = set(expected_includes) - set(all_includes)
        print(f"✗ Include collection failed: missing {missing}")
        return False

    return True

def test_practical_modules():
    """Test the practical module definitions."""
    print("\nTesting Practical Module Definitions...")

    from module_registry import ModuleRegistry

    registry = ModuleRegistry()

    # Register crypto module (our first converted module)
    crypto = registry.register_module(
        name='crypto',
        description='Cryptographic functions for secure boot and signing',
        sources=['rsa.c', 'sha.c'],
        headers=['rsa.h', 'sha.h', 'hash-internal.h'],
        includes=['.'],
        dependencies=[],
        directory='crypto'
    )

    # Check that files exist
    crypto_dir = Path('crypto')
    missing_files = []

    for source in crypto.sources:
        file_path = crypto_dir / source
        if not file_path.exists():
            missing_files.append(str(file_path))

    for header in crypto.headers:
        file_path = crypto_dir / header
        if not file_path.exists():
            missing_files.append(str(file_path))

    if missing_files:
        print(f"✗ Missing files: {missing_files}")
        return False
    else:
        print("✓ All crypto module files exist")

    # Test dependency validation
    try:
        registry.validate_all_dependencies()
        print("✓ Module dependencies are valid")
    except Exception as e:
        print(f"✗ Dependency validation failed: {e}")
        return False

    return True

def test_incremental_compatibility():
    """Test compatibility with the existing build system."""
    print("\nTesting Incremental Compatibility...")

    # Check that key files exist for incremental build
    required_files = [
        'SConscript.incremental',
        'modules/module_registry.py',
        'board/main.c',
        'board/jungle/main.c',
        'crypto/rsa.c',
        'crypto/sha.c',
    ]

    missing_files = []
    for file_path in required_files:
        if not os.path.exists(file_path):
            missing_files.append(file_path)

    if missing_files:
        print(f"✗ Missing required files: {missing_files}")
        return False
    else:
        print("✓ All required files for incremental build exist")

    # Test that the incremental script is syntactically valid
    try:
        with open('SConscript.incremental', 'r') as f:
            script_content = f.read()

        # Basic syntax check
        compile(script_content, 'SConscript.incremental', 'exec')
        print("✓ Incremental build script is syntactically valid")
    except SyntaxError as e:
        print(f"✗ Incremental build script has syntax errors: {e}")
        return False

    return True

def test_migration_strategy():
    """Test the migration strategy."""
    print("\nTesting Migration Strategy...")

    # Check migration plan components
    migration_components = [
        ('Modular PoC', 'SConscript.modular'),
        ('Dependency Analysis', 'practical_dependency_mapping.py'),
        ('Module Registry', 'modules/module_registry.py'),
        ('Incremental Build', 'SConscript.incremental'),
        ('Validation Tools', 'validate_incremental.py'),
    ]

    missing_components = []
    for name, file_path in migration_components:
        if not os.path.exists(file_path):
            missing_components.append(f"{name} ({file_path})")

    if missing_components:
        print(f"✗ Missing migration components: {missing_components}")
        return False
    else:
        print("✓ All migration strategy components are present")

    # Check that we can load the practical dependency mapping
    try:
        sys.path.insert(0, '.')
        import practical_dependency_mapping
        mapper = practical_dependency_mapping.PandaModuleMapper()
        mapper.define_modules()
        targets = mapper.generate_target_configs()

        if 'panda_h7' in targets and 'panda_jungle_h7' in targets:
            print("✓ Practical dependency mapping works correctly")
        else:
            print("✗ Practical dependency mapping missing expected targets")
            return False

    except Exception as e:
        print(f"✗ Practical dependency mapping failed: {e}")
        return False

    return True

def run_full_validation():
    """Run the complete validation suite."""
    print("="*60)
    print("INCREMENTAL MODULAR REFACTORING VALIDATION")
    print("="*60)

    tests = [
        ("Module Registry", test_module_registry),
        ("Practical Modules", test_practical_modules),
        ("Incremental Compatibility", test_incremental_compatibility),
        ("Migration Strategy", test_migration_strategy),
    ]

    results = []
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append(result)
            status = "PASS" if result else "FAIL"
            print(f"\n{test_name}: {status}")
        except Exception as e:
            print(f"\n{test_name}: FAIL (Exception: {e})")
            results.append(False)

    print("\n" + "="*60)
    print("VALIDATION SUMMARY")
    print("="*60)

    passed = sum(results)
    total = len(results)

    print(f"Tests passed: {passed}/{total}")

    if all(results):
        print("✓ All validation tests passed!")
        print("✓ Incremental modular refactoring is ready")
        print("✓ Safe to proceed with gradual migration")

        print("\nNEXT STEPS:")
        print("1. Convert one module at a time (start with crypto)")
        print("2. Validate each conversion with output comparison")
        print("3. Add module-level unit tests")
        print("4. Gradually migrate remaining modules")
        print("5. Remove legacy build system when complete")

        return True
    else:
        print("✗ Some validation tests failed")
        print("✗ Address failures before proceeding")
        return False

def main():
    """Main validation entry point."""
    success = run_full_validation()
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())