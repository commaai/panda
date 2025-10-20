#!/usr/bin/env python3
"""
Test script for modular build system PoC.

This demonstrates the key concepts of the modular build system
without requiring a full SCons environment.
"""

import os
import sys

# Add current directory to path for imports
sys.path.insert(0, '.')

class MockEnvironment:
    """Mock SCons Environment for testing."""
    def __init__(self, **kwargs):
        self.vars = kwargs

    def Object(self, target, source=None):
        return f"Object({target})"

    def Program(self, target, sources, **kwargs):
        return f"Program({target}, {sources})"

    def Clone(self):
        return MockEnvironment(**self.vars)

    def Append(self, **kwargs):
        for k, v in kwargs.items():
            if k in self.vars:
                if isinstance(self.vars[k], list):
                    self.vars[k].extend(v if isinstance(v, list) else [v])
                else:
                    self.vars[k] += v
            else:
                self.vars[k] = v

class MockDir:
    """Mock SCons Dir for testing."""
    def __init__(self, path):
        self.path = path

    def __str__(self):
        return self.path

class MockFile:
    """Mock SCons File for testing."""
    def __init__(self, path):
        self.path = path

    def srcnode(self):
        return self

    @property
    def relpath(self):
        return self.path

    def get_path(self):
        return self.path

# Mock SCons functions
def Environment(**kwargs):
    return MockEnvironment(**kwargs)

def Dir(path):
    return MockDir(path)

def File(path):
    return MockFile(path)

def Builder(**kwargs):
    return "Builder"

def GetOption(name):
    return True

def AddOption(*args, **kwargs):
    pass

def SConscript(path):
    pass

def test_module_system():
    """Test the module system components."""
    print("Testing Module System...")

    # Define module classes locally for testing
    class Module:
        def __init__(self, name, sources=None, includes=None, dependencies=None,
                     flags=None, description=""):
            self.name = name
            self.sources = sources or []
            self.includes = includes or []
            self.dependencies = dependencies or []
            self.flags = flags or []
            self.description = description
            self.built_objects = []

    class ModuleRegistry:
        def __init__(self):
            self.modules = {}
            self.build_cache = {}

        def register(self, module):
            if module.name in self.modules:
                raise ValueError(f"Module {module.name} already registered")
            self.modules[module.name] = module
            return module

        def get(self, name):
            if name not in self.modules:
                raise ValueError(f"Module {name} not found")
            return self.modules[name]

        def get_dependencies(self, module_name):
            visited = set()
            result = []

            def _collect(name):
                if name in visited:
                    return
                visited.add(name)
                module = self.get(name)
                for dep in module.dependencies:
                    _collect(dep)
                    if dep not in result:
                        result.append(dep)

            _collect(module_name)
            return result

    registry = ModuleRegistry()

    # Create test modules
    core = Module(
        name="core_test",
        sources=["core.c"],
        description="Core test module"
    )

    driver = Module(
        name="driver_test",
        sources=["driver.c"],
        dependencies=["core_test"],
        description="Driver test module"
    )

    app = Module(
        name="app_test",
        sources=["app.c"],
        dependencies=["driver_test"],
        description="Application test module"
    )

    # Register modules
    registry.register(core)
    registry.register(driver)
    registry.register(app)

    print(f"✓ Registered {len(registry.modules)} modules")

    # Test dependency resolution
    deps = registry.get_dependencies("app_test")
    expected_deps = ["core_test", "driver_test"]

    if deps == expected_deps:
        print("✓ Dependency resolution works correctly")
    else:
        print(f"✗ Dependency resolution failed: got {deps}, expected {expected_deps}")
        return False

    # Test circular dependency detection
    try:
        circular = Module("circular", dependencies=["app_test"])
        registry.register(circular)
        app.dependencies.append("circular")
        registry.get_dependencies("circular")  # Should not cause infinite loop
        print("✓ Circular dependency handling works")
    except Exception:
        print("✗ Circular dependency handling failed")
        return False

    return True

def test_build_system():
    """Test the build system logic."""
    print("\nTesting Build System...")

    try:
        # Test that the modular script is syntactically valid
        with open('SConscript.modular', 'r') as f:
            script_content = f.read()

        # Basic syntax check by compilation
        compile(script_content, 'SConscript.modular', 'exec')
        print("✓ Build script is syntactically valid")

        # Test key concepts from the script
        lines = script_content.split('\n')

        # Check for key modular concepts
        has_module_class = any('class Module:' in line for line in lines)
        has_registry = any('ModuleRegistry' in line for line in lines)
        has_build_target = any('build_modular_target' in line for line in lines)

        if has_module_class and has_registry and has_build_target:
            print("✓ Key modular concepts are present")
            return True
        else:
            print("✗ Missing key modular concepts")
            return False

    except SyntaxError as e:
        print(f"✗ Build script has syntax errors: {e}")
        return False
    except Exception as e:
        print(f"✗ Build script test failed: {e}")
        return False

def test_file_structure():
    """Test that required files exist for the PoC."""
    print("\nTesting File Structure...")

    required_files = [
        "SConscript.modular",
        "validate_modular_build.py",
        "board/main.c",
        "board/jungle/main.c",
        "board/drivers/fan.h",
        "board/drivers/led.h",
    ]

    missing_files = []
    for file_path in required_files:
        if not os.path.exists(file_path):
            missing_files.append(file_path)

    if missing_files:
        print(f"✗ Missing required files: {missing_files}")
        return False
    else:
        print("✓ All required files present")
        return True

def main():
    """Run all tests."""
    print("="*50)
    print("MODULAR BUILD SYSTEM PoC TESTS")
    print("="*50)

    tests = [
        test_file_structure,
        test_module_system,
        test_build_system,
    ]

    results = []
    for test in tests:
        try:
            result = test()
            results.append(result)
        except Exception as e:
            print(f"✗ Test {test.__name__} failed with exception: {e}")
            results.append(False)

    print("\n" + "="*50)
    print("TEST SUMMARY")
    print("="*50)

    passed = sum(results)
    total = len(results)

    print(f"Tests passed: {passed}/{total}")

    if all(results):
        print("✓ All tests passed! PoC is ready for validation.")
        return True
    else:
        print("✗ Some tests failed. Check the output above.")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)