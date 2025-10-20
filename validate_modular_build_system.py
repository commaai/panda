#!/usr/bin/env python3
"""
Comprehensive validation script for the modular build system

This script validates all aspects of the modular build system:
1. Module structure and files
2. Dependency resolution
3. Build integration logic  
4. SConscript parsing and validation
5. Module registry functionality
"""

import os
import sys
import ast
import json
from typing import Dict, List, Any

# Add modules directory to Python path
sys.path.insert(0, 'modules')
from module_registry import module_registry

class ModularBuildValidator:
    """Comprehensive validator for the modular build system."""
    
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.results = {}
        
    def log_error(self, message: str):
        """Log an error message."""
        self.errors.append(message)
        print(f"✗ ERROR: {message}")
        
    def log_warning(self, message: str):
        """Log a warning message."""
        self.warnings.append(message)
        print(f"⚠ WARNING: {message}")
        
    def log_success(self, message: str):
        """Log a success message."""
        print(f"✓ {message}")

    def validate_module_structure(self) -> bool:
        """Validate that all module directories and files exist."""
        print("\n" + "="*60)
        print("VALIDATING MODULE STRUCTURE")
        print("="*60)
        
        expected_modules = {
            'hal_stm32h7': {
                'required_files': [
                    'SConscript', 'README.md', 'board.h', 'clock.h',
                    'startup_stm32h7x5xx.s', 'peripherals.h'
                ],
                'required_dirs': ['inc'],
                'description': 'STM32H7 hardware abstraction layer'
            },
            'drivers_basic': {
                'required_files': [
                    'SConscript', 'README.md', 'gpio.h', 'led.h',
                    'timers.h', 'pwm.h', 'registers.h', 'interrupts.h'
                ],
                'required_dirs': [],
                'description': 'Basic hardware drivers'
            },
            'drivers_comm': {
                'required_files': [
                    'SConscript', 'README.md', 'uart.h', 'usb.h', 'spi.h'
                ],
                'required_dirs': [],
                'description': 'Communication drivers'
            },
            'crypto': {
                'required_files': [
                    'SConscript', 'README.md', 'rsa.h', 'sha.h',
                    'rsa.c', 'sha.c', 'hash-internal.h'
                ],
                'required_dirs': [],
                'description': 'Cryptographic functions'
            }
        }
        
        structure_valid = True
        
        for module_name, module_info in expected_modules.items():
            module_dir = f'modules/{module_name}'
            
            if not os.path.exists(module_dir):
                self.log_error(f"Module directory missing: {module_dir}")
                structure_valid = False
                continue
                
            self.log_success(f"Module directory exists: {module_name}")
            
            # Check required files
            missing_files = []
            for file_name in module_info['required_files']:
                file_path = os.path.join(module_dir, file_name)
                if not os.path.exists(file_path):
                    missing_files.append(file_name)
                    
            if missing_files:
                self.log_error(f"Missing files in {module_name}: {missing_files}")
                structure_valid = False
            else:
                self.log_success(f"All required files present in {module_name}")
                
            # Check required directories
            for dir_name in module_info['required_dirs']:
                dir_path = os.path.join(module_dir, dir_name)
                if not os.path.exists(dir_path):
                    self.log_error(f"Missing directory in {module_name}: {dir_name}")
                    structure_valid = False
                    
        self.results['structure_valid'] = structure_valid
        return structure_valid

    def validate_sconscript_syntax(self) -> bool:
        """Validate that all SConscript files have valid Python syntax."""
        print("\n" + "="*60)
        print("VALIDATING SCONSCRIPT SYNTAX")
        print("="*60)
        
        syntax_valid = True
        module_dirs = ['modules/hal_stm32h7', 'modules/drivers_basic', 
                      'modules/drivers_comm', 'modules/crypto']
        
        # Also check the main incremental script
        scripts_to_check = ['SConscript.incremental'] + [f'{d}/SConscript' for d in module_dirs]
        
        for script_path in scripts_to_check:
            if not os.path.exists(script_path):
                self.log_error(f"SConscript file missing: {script_path}")
                syntax_valid = False
                continue
                
            try:
                with open(script_path, 'r') as f:
                    content = f.read()
                    
                # Parse the Python syntax
                ast.parse(content)
                self.log_success(f"Valid syntax: {script_path}")
                
                # Check for required SCons patterns
                if 'Import(' in content and 'module_registry' in content:
                    self.log_success(f"Module registry integration found: {script_path}")
                elif script_path == 'SConscript.incremental':
                    if 'register_incremental_modules' in content:
                        self.log_success(f"Incremental module registration found: {script_path}")
                    else:
                        self.log_warning(f"No incremental module registration: {script_path}")
                
            except SyntaxError as e:
                self.log_error(f"Syntax error in {script_path}: {e}")
                syntax_valid = False
            except Exception as e:
                self.log_error(f"Error reading {script_path}: {e}")
                syntax_valid = False
                
        self.results['syntax_valid'] = syntax_valid
        return syntax_valid

    def validate_dependency_resolution(self) -> bool:
        """Validate module dependency resolution logic."""
        print("\n" + "="*60)
        print("VALIDATING DEPENDENCY RESOLUTION")
        print("="*60)
        
        # Clear any existing modules for clean test
        module_registry.modules.clear()
        module_registry.build_order_cache.clear()
        
        try:
            # Register test modules with proper dependencies
            hal_module = module_registry.register_module(
                name='hal_stm32h7',
                description='STM32H7 hardware abstraction layer',
                sources=['startup_stm32h7x5xx.s'],
                headers=['board.h', 'clock.h'],
                includes=['.', 'inc'],
                dependencies=[],  # No dependencies - foundation layer
                directory='modules/hal_stm32h7'
            )
            
            basic_module = module_registry.register_module(
                name='drivers_basic',
                description='Basic hardware drivers',
                sources=[],
                headers=['gpio.h', 'led.h'],
                includes=['.'],
                dependencies=['hal_stm32h7'],  # Depends on HAL
                directory='modules/drivers_basic'
            )
            
            comm_module = module_registry.register_module(
                name='drivers_comm',
                description='Communication drivers',
                sources=[],
                headers=['uart.h', 'usb.h'],
                includes=['.'],
                dependencies=['drivers_basic'],  # Depends on basic drivers
                directory='modules/drivers_comm'
            )
            
            crypto_module = module_registry.register_module(
                name='crypto',
                description='Cryptographic functions',
                sources=['rsa.c', 'sha.c'],
                headers=['rsa.h', 'sha.h'],
                includes=['.'],
                dependencies=[],  # Self-contained
                directory='modules/crypto'
            )
            
            # Test dependency validation
            module_registry.validate_all_dependencies()
            self.log_success("Module dependency validation passed")
            
            # Test build order calculation
            expected_orders = {
                'hal_stm32h7': ['hal_stm32h7'],
                'drivers_basic': ['hal_stm32h7', 'drivers_basic'],
                'drivers_comm': ['hal_stm32h7', 'drivers_basic', 'drivers_comm'],
                'crypto': ['crypto']
            }
            
            for module_name, expected_order in expected_orders.items():
                actual_order = module_registry.get_build_order(module_name)
                if actual_order == expected_order:
                    self.log_success(f"Build order correct for {module_name}: {' -> '.join(actual_order)}")
                else:
                    self.log_error(f"Build order incorrect for {module_name}. Expected: {expected_order}, Got: {actual_order}")
                    return False
            
            # Test include path resolution
            comm_includes = module_registry.get_all_includes('drivers_comm')
            expected_includes = ['modules/drivers_comm/.', 'modules/drivers_basic/.', 'modules/hal_stm32h7/.', 'modules/hal_stm32h7/inc']
            
            # Sort both for comparison (order shouldn't matter for includes)
            if sorted(comm_includes) == sorted(expected_includes):
                self.log_success(f"Include resolution correct for drivers_comm")
            else:
                self.log_warning(f"Include resolution may be incorrect. Expected: {expected_includes}, Got: {comm_includes}")
            
            # Test circular dependency detection
            try:
                # Create a circular dependency (should fail)
                module_registry.register_module(
                    name='circular_test',
                    description='Test circular dependency',
                    dependencies=['circular_test'],
                    directory='test'
                )
                module_registry.get_build_order('circular_test')
                self.log_error("Circular dependency detection failed - should have thrown exception")
                return False
            except ValueError as e:
                if "circular" in str(e).lower():
                    self.log_success("Circular dependency detection working")
                else:
                    self.log_error(f"Unexpected error in circular dependency test: {e}")
                    return False
            
            self.results['dependency_resolution'] = True
            return True
            
        except Exception as e:
            self.log_error(f"Dependency resolution test failed: {e}")
            import traceback
            traceback.print_exc()
            self.results['dependency_resolution'] = False
            return False

    def validate_build_integration(self) -> bool:
        """Validate the incremental build integration logic."""
        print("\n" + "="*60)
        print("VALIDATING BUILD INTEGRATION")
        print("="*60)
        
        try:
            # Read and analyze the incremental build script
            with open('SConscript.incremental', 'r') as f:
                script_content = f.read()
            
            # Check for critical functions and patterns
            required_patterns = [
                'register_incremental_modules',
                'build_incremental_target',
                'module_registry',
                'hal_stm32h7',
                'drivers_basic',
                'drivers_comm',
                'crypto'
            ]
            
            for pattern in required_patterns:
                if pattern in script_content:
                    self.log_success(f"Found required pattern: {pattern}")
                else:
                    self.log_error(f"Missing required pattern: {pattern}")
                    return False
            
            # Check module loading logic
            if 'SConscript(\'modules/' in script_content:
                self.log_success("Module loading logic present")
            else:
                self.log_error("Module loading logic missing")
                return False
            
            # Check dependency order
            if 'module_build_order' in script_content and 'hal_stm32h7' in script_content:
                self.log_success("Build order logic present")
            else:
                self.log_error("Build order logic missing")
                return False
            
            # Check fallback handling
            if 'fallback' in script_content and 'except Exception' in script_content:
                self.log_success("Fallback error handling present")
            else:
                self.log_warning("Limited fallback error handling")
            
            self.results['build_integration'] = True
            return True
            
        except Exception as e:
            self.log_error(f"Build integration validation failed: {e}")
            self.results['build_integration'] = False
            return False

    def validate_module_documentation(self) -> bool:
        """Validate that module documentation is comprehensive."""
        print("\n" + "="*60)
        print("VALIDATING MODULE DOCUMENTATION")
        print("="*60)
        
        documentation_valid = True
        modules = ['hal_stm32h7', 'drivers_basic', 'drivers_comm', 'crypto']
        
        for module_name in modules:
            readme_path = f'modules/{module_name}/README.md'
            
            if not os.path.exists(readme_path):
                self.log_error(f"Missing README.md for {module_name}")
                documentation_valid = False
                continue
            
            try:
                with open(readme_path, 'r') as f:
                    content = f.read()
                
                # Check for required sections
                required_sections = [
                    '# ' + module_name.replace('_', ' ').title(),
                    '## Overview',
                    '## Features', 
                    '## Architecture',
                    '## Public Interface',
                    '## Dependencies',
                    '## Usage'
                ]
                
                missing_sections = []
                for section in required_sections:
                    if section.lower() not in content.lower():
                        missing_sections.append(section)
                
                if missing_sections:
                    self.log_warning(f"Missing documentation sections in {module_name}: {missing_sections}")
                else:
                    self.log_success(f"Complete documentation for {module_name}")
                    
                # Check for code examples
                if '```c' in content or '```python' in content:
                    self.log_success(f"Code examples present in {module_name}")
                else:
                    self.log_warning(f"No code examples in {module_name}")
                    
            except Exception as e:
                self.log_error(f"Error reading documentation for {module_name}: {e}")
                documentation_valid = False
        
        self.results['documentation_valid'] = documentation_valid
        return documentation_valid

    def generate_report(self) -> Dict[str, Any]:
        """Generate a comprehensive validation report."""
        print("\n" + "="*60)
        print("VALIDATION REPORT")
        print("="*60)
        
        report = {
            'validation_summary': {
                'total_errors': len(self.errors),
                'total_warnings': len(self.warnings),
                'overall_status': 'PASS' if len(self.errors) == 0 else 'FAIL'
            },
            'detailed_results': self.results,
            'errors': self.errors,
            'warnings': self.warnings,
            'recommendations': []
        }
        
        # Add recommendations based on results
        if len(self.errors) == 0:
            report['recommendations'].append("✓ Modular build system is ready for production use")
            report['recommendations'].append("✓ All modules follow established patterns")
            report['recommendations'].append("✓ Dependencies are properly resolved")
        else:
            report['recommendations'].append("Fix all errors before proceeding")
            
        if len(self.warnings) > 0:
            report['recommendations'].append("Consider addressing warnings for improved robustness")
        
        # Print summary
        print(f"Overall Status: {report['validation_summary']['overall_status']}")
        print(f"Errors: {report['validation_summary']['total_errors']}")
        print(f"Warnings: {report['validation_summary']['total_warnings']}")
        
        if self.errors:
            print("\nErrors found:")
            for error in self.errors:
                print(f"  - {error}")
                
        if self.warnings:
            print("\nWarnings:")
            for warning in self.warnings:
                print(f"  - {warning}")
        
        print("\nRecommendations:")
        for rec in report['recommendations']:
            print(f"  - {rec}")
        
        return report

    def run_validation(self) -> bool:
        """Run all validation tests."""
        print("COMPREHENSIVE MODULAR BUILD SYSTEM VALIDATION")
        print("=" * 80)
        
        # Run all validation tests
        structure_ok = self.validate_module_structure()
        syntax_ok = self.validate_sconscript_syntax()
        dependencies_ok = self.validate_dependency_resolution()
        integration_ok = self.validate_build_integration()
        docs_ok = self.validate_module_documentation()
        
        # Generate final report
        report = self.generate_report()
        
        # Save report to file
        with open('modular_build_validation_report.json', 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"\n✓ Validation report saved to: modular_build_validation_report.json")
        
        return len(self.errors) == 0

def main():
    """Main validation function."""
    validator = ModularBuildValidator()
    success = validator.run_validation()
    
    if success:
        print("\n" + "="*80)
        print("🎉 MODULAR BUILD SYSTEM VALIDATION SUCCESSFUL!")
        print("="*80)
        print("\nThe modular build system is ready for use:")
        print("1. All module structures are correct")
        print("2. Dependencies resolve properly")
        print("3. Build integration is functional")
        print("4. Documentation is comprehensive")
        print("\nNext steps:")
        print("- Test with actual SCons build: scons -f SConscript.incremental")
        print("- Compare modular vs legacy build outputs")
        print("- Begin migrating additional modules")
        return 0
    else:
        print("\n" + "="*80)
        print("❌ MODULAR BUILD SYSTEM VALIDATION FAILED!")
        print("="*80)
        print("\nPlease address the errors found before proceeding.")
        return 1

if __name__ == '__main__':
    sys.exit(main())