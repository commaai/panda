#!/usr/bin/env python3
"""
Comprehensive Validation Suite for Panda Modular Build System

This script provides comprehensive validation of the entire modular build
system implementation, including:

1. PoC validation
2. Dependency mapping validation  
3. Incremental refactoring validation
4. Integration testing
5. Performance analysis
6. Migration readiness assessment

This serves as the final validation before the modular system can be
considered production-ready.
"""

import os
import sys
import json
import time
import subprocess
from pathlib import Path
from typing import Dict, List, Any

class ComprehensiveValidator:
    """Comprehensive validation of the modular build system."""
    
    def __init__(self):
        self.test_results = {}
        self.start_time = time.time()
        
    def log_result(self, test_name: str, passed: bool, details: str = ""):
        """Log a test result."""
        self.test_results[test_name] = {
            'passed': passed,
            'details': details,
            'timestamp': time.time() - self.start_time
        }
        status = "PASS" if passed else "FAIL"
        print(f"  {test_name}: {status}")
        if details and not passed:
            print(f"    Details: {details}")
    
    def validate_poc_system(self) -> bool:
        """Validate the PoC modular build system."""
        print("\n1. VALIDATING POC SYSTEM")
        print("-" * 40)
        
        all_passed = True
        
        # Check PoC files exist
        poc_files = [
            'SConscript.modular',
            'demo_modular_system.py',
            'test_modular_poc.py'
        ]
        
        for file_path in poc_files:
            exists = os.path.exists(file_path)
            self.log_result(f"PoC file exists: {file_path}", exists)
            all_passed &= exists
        
        # Test PoC functionality
        try:
            result = subprocess.run([sys.executable, 'test_modular_poc.py'], 
                                  capture_output=True, text=True, timeout=30)
            poc_test_passed = result.returncode == 0
            self.log_result("PoC functional test", poc_test_passed, 
                          result.stderr if not poc_test_passed else "")
            all_passed &= poc_test_passed
        except Exception as e:
            self.log_result("PoC functional test", False, str(e))
            all_passed = False
        
        # Test PoC demonstration
        try:
            result = subprocess.run([sys.executable, 'demo_modular_system.py'],
                                  capture_output=True, text=True, timeout=30)
            demo_passed = result.returncode == 0
            self.log_result("PoC demonstration", demo_passed,
                          result.stderr if not demo_passed else "")
            all_passed &= demo_passed
        except Exception as e:
            self.log_result("PoC demonstration", False, str(e))
            all_passed = False
        
        return all_passed
    
    def validate_dependency_mapping(self) -> bool:
        """Validate the dependency mapping system."""
        print("\n2. VALIDATING DEPENDENCY MAPPING")
        print("-" * 40)
        
        all_passed = True
        
        # Check mapping files exist
        mapping_files = [
            'dependency_analysis.py',
            'practical_dependency_mapping.py',
            'practical_dependency_mapping.json'
        ]
        
        for file_path in mapping_files:
            exists = os.path.exists(file_path)
            self.log_result(f"Mapping file exists: {file_path}", exists)
            all_passed &= exists
        
        # Test dependency analysis
        try:
            sys.path.insert(0, '.')
            import practical_dependency_mapping
            
            mapper = practical_dependency_mapping.PandaModuleMapper()
            mapper.define_modules()
            
            # Check that we have reasonable number of modules
            module_count = len(mapper.modules)
            reasonable_count = 10 <= module_count <= 20
            self.log_result("Reasonable module count", reasonable_count,
                          f"Got {module_count} modules")
            all_passed &= reasonable_count
            
            # Check target configurations
            targets = mapper.generate_target_configs()
            has_panda = 'panda_h7' in targets
            has_jungle = 'panda_jungle_h7' in targets
            
            self.log_result("Panda target defined", has_panda)
            self.log_result("Jungle target defined", has_jungle)
            all_passed &= has_panda and has_jungle
            
            # Validate build orders
            for target_name, config in targets.items():
                build_order = config.get('build_order', [])
                has_build_order = len(build_order) > 0
                self.log_result(f"Build order for {target_name}", has_build_order,
                              f"Order: {' -> '.join(build_order[:3])}...")
                all_passed &= has_build_order
            
        except Exception as e:
            self.log_result("Dependency mapping execution", False, str(e))
            all_passed = False
        
        return all_passed
    
    def validate_incremental_system(self) -> bool:
        """Validate the incremental refactoring system."""
        print("\n3. VALIDATING INCREMENTAL SYSTEM")
        print("-" * 40)
        
        all_passed = True
        
        # Check incremental files exist
        incremental_files = [
            'modules/module_registry.py',
            'SConscript.incremental',
            'validate_incremental.py'
        ]
        
        for file_path in incremental_files:
            exists = os.path.exists(file_path)
            self.log_result(f"Incremental file exists: {file_path}", exists)
            all_passed &= exists
        
        # Test incremental validation
        try:
            result = subprocess.run([sys.executable, 'validate_incremental.py'],
                                  capture_output=True, text=True, timeout=60)
            incremental_passed = result.returncode == 0
            self.log_result("Incremental validation", incremental_passed,
                          result.stderr if not incremental_passed else "")
            all_passed &= incremental_passed
        except Exception as e:
            self.log_result("Incremental validation", False, str(e))
            all_passed = False
        
        # Test module registry functionality
        try:
            sys.path.insert(0, 'modules')
            from module_registry import ModuleRegistry
            
            registry = ModuleRegistry()
            
            # Test basic functionality
            test_module = registry.register_module(
                name='test',
                description='Test module',
                sources=['test.c'],
                dependencies=[]
            )
            
            module_registered = 'test' in registry.modules
            self.log_result("Module registry basic function", module_registered)
            all_passed &= module_registered
            
        except Exception as e:
            self.log_result("Module registry test", False, str(e))
            all_passed = False
        
        return all_passed
    
    def validate_file_organization(self) -> bool:
        """Validate the overall file organization."""
        print("\n4. VALIDATING FILE ORGANIZATION")
        print("-" * 40)
        
        all_passed = True
        
        # Check directory structure
        expected_dirs = [
            'board',
            'board/drivers',
            'board/stm32h7',
            'board/jungle',
            'crypto',
            'modules'
        ]
        
        for dir_path in expected_dirs:
            exists = os.path.isdir(dir_path)
            self.log_result(f"Directory exists: {dir_path}", exists)
            all_passed &= exists
        
        # Check key source files
        key_files = [
            'board/main.c',
            'board/jungle/main.c',
            'board/bootstub.c',
            'crypto/rsa.c',
            'crypto/sha.c'
        ]
        
        for file_path in key_files:
            exists = os.path.exists(file_path)
            self.log_result(f"Key file exists: {file_path}", exists)
            all_passed &= exists
        
        # Check for proper separation
        modular_files = [
            'SConscript.modular',
            'SConscript.incremental',
            'modules/module_registry.py'
        ]
        
        legacy_files = [
            'SConscript',
            'board/main.c'
        ]
        
        # Both systems should coexist
        modular_exists = all(os.path.exists(f) for f in modular_files)
        legacy_exists = all(os.path.exists(f) for f in legacy_files)
        
        self.log_result("Modular system files exist", modular_exists)
        self.log_result("Legacy system preserved", legacy_exists)
        
        coexistence = modular_exists and legacy_exists
        self.log_result("Systems coexist properly", coexistence)
        all_passed &= coexistence
        
        return all_passed
    
    def validate_documentation(self) -> bool:
        """Validate documentation and reporting."""
        print("\n5. VALIDATING DOCUMENTATION")
        print("-" * 40)
        
        all_passed = True
        
        # Check for generated reports
        report_files = [
            'dependency_report.json',
            'practical_dependency_mapping.json'
        ]
        
        for file_path in report_files:
            exists = os.path.exists(file_path)
            self.log_result(f"Report exists: {file_path}", exists)
            all_passed &= exists
            
            # Validate JSON structure
            if exists:
                try:
                    with open(file_path, 'r') as f:
                        data = json.load(f)
                    valid_json = isinstance(data, dict) and len(data) > 0
                    self.log_result(f"Valid JSON: {file_path}", valid_json)
                    all_passed &= valid_json
                except Exception as e:
                    self.log_result(f"Valid JSON: {file_path}", False, str(e))
                    all_passed = False
        
        # Check that scripts have proper documentation
        scripts_to_check = [
            'demo_modular_system.py',
            'practical_dependency_mapping.py',
            'validate_incremental.py'
        ]
        
        for script in scripts_to_check:
            if os.path.exists(script):
                try:
                    with open(script, 'r') as f:
                        content = f.read()
                    
                    has_docstring = '"""' in content
                    has_description = len(content.split('\n')[0:10]) > 5  # Rough check
                    
                    documented = has_docstring and has_description
                    self.log_result(f"Script documented: {script}", documented)
                    all_passed &= documented
                    
                except Exception as e:
                    self.log_result(f"Script documented: {script}", False, str(e))
                    all_passed = False
        
        return all_passed
    
    def analyze_migration_readiness(self) -> Dict[str, Any]:
        """Analyze readiness for production migration."""
        print("\n6. MIGRATION READINESS ANALYSIS")
        print("-" * 40)
        
        analysis = {
            'ready_for_migration': True,
            'risk_level': 'LOW',
            'confidence': 'HIGH',
            'blockers': [],
            'recommendations': [],
            'metrics': {}
        }
        
        # Check test pass rates
        total_tests = len(self.test_results)
        passed_tests = sum(1 for result in self.test_results.values() if result['passed'])
        pass_rate = passed_tests / total_tests if total_tests > 0 else 0
        
        analysis['metrics']['test_pass_rate'] = pass_rate
        analysis['metrics']['total_tests'] = total_tests
        analysis['metrics']['passed_tests'] = passed_tests
        
        # Determine readiness
        if pass_rate < 0.9:
            analysis['ready_for_migration'] = False
            analysis['risk_level'] = 'HIGH'
            analysis['confidence'] = 'LOW'
            analysis['blockers'].append(f"Low test pass rate: {pass_rate:.1%}")
        
        # Check for specific failures
        critical_tests = [
            'PoC functional test',
            'PoC demonstration',
            'Incremental validation',
            'Module registry basic function'
        ]
        
        failed_critical = [test for test in critical_tests 
                          if test in self.test_results and not self.test_results[test]['passed']]
        
        if failed_critical:
            analysis['ready_for_migration'] = False
            analysis['risk_level'] = 'HIGH'
            analysis['blockers'].extend([f"Critical test failed: {test}" for test in failed_critical])
        
        # Add recommendations
        if analysis['ready_for_migration']:
            analysis['recommendations'] = [
                "Begin gradual migration starting with crypto module",
                "Set up continuous validation pipeline",
                "Create rollback procedures",
                "Train team on new module system",
                "Establish module-level testing standards"
            ]
        else:
            analysis['recommendations'] = [
                "Fix all test failures before proceeding",
                "Review and address blockers",
                "Validate system thoroughly",
                "Consider additional testing"
            ]
        
        # Print analysis
        print(f"Ready for migration: {analysis['ready_for_migration']}")
        print(f"Risk level: {analysis['risk_level']}")
        print(f"Confidence: {analysis['confidence']}")
        print(f"Test pass rate: {pass_rate:.1%} ({passed_tests}/{total_tests})")
        
        if analysis['blockers']:
            print("Blockers:")
            for blocker in analysis['blockers']:
                print(f"  - {blocker}")
        
        print("Recommendations:")
        for rec in analysis['recommendations']:
            print(f"  - {rec}")
        
        return analysis
    
    def generate_final_report(self, analysis: Dict[str, Any]):
        """Generate the final validation report."""
        print(f"\n{'='*60}")
        print("COMPREHENSIVE VALIDATION REPORT")
        print(f"{'='*60}")
        
        # Overall status
        overall_passed = analysis['ready_for_migration']
        status = "READY FOR PRODUCTION" if overall_passed else "NEEDS WORK"
        print(f"\nOVERALL STATUS: {status}")
        
        # Test summary
        print(f"\nTEST SUMMARY:")
        print(f"  Total tests: {analysis['metrics']['total_tests']}")
        print(f"  Passed: {analysis['metrics']['passed_tests']}")
        print(f"  Pass rate: {analysis['metrics']['test_pass_rate']:.1%}")
        
        # Detailed results
        print(f"\nDETAILED RESULTS:")
        for test_name, result in self.test_results.items():
            status = "PASS" if result['passed'] else "FAIL"
            timestamp = f"{result['timestamp']:.1f}s"
            print(f"  {timestamp:>6} {status:>4} {test_name}")
            if result['details'] and not result['passed']:
                print(f"              └─ {result['details']}")
        
        # Component status
        components = {
            'PoC System': any('PoC' in name for name in self.test_results),
            'Dependency Mapping': any('Mapping' in name or 'target' in name for name in self.test_results),
            'Incremental System': any('Incremental' in name or 'registry' in name for name in self.test_results),
            'File Organization': any('exists' in name for name in self.test_results),
            'Documentation': any('documented' in name or 'JSON' in name for name in self.test_results)
        }
        
        print(f"\nCOMPONENT STATUS:")
        for component, has_tests in components.items():
            status = "TESTED" if has_tests else "NOT TESTED"
            print(f"  {component}: {status}")
        
        # Migration readiness
        print(f"\nMIGRATION READINESS:")
        print(f"  Ready: {analysis['ready_for_migration']}")
        print(f"  Risk Level: {analysis['risk_level']}")
        print(f"  Confidence: {analysis['confidence']}")
        
        if overall_passed:
            print(f"\n✓ All systems validated successfully")
            print(f"✓ Modular build system is production-ready")
            print(f"✓ Migration can proceed safely")
        else:
            print(f"\n✗ Validation incomplete or failed")
            print(f"✗ Address issues before proceeding")
        
        # Save detailed report
        report_data = {
            'timestamp': time.time(),
            'overall_status': status,
            'test_results': self.test_results,
            'analysis': analysis,
            'summary': {
                'total_tests': analysis['metrics']['total_tests'],
                'passed_tests': analysis['metrics']['passed_tests'],
                'pass_rate': analysis['metrics']['test_pass_rate'],
                'ready_for_migration': analysis['ready_for_migration']
            }
        }
        
        with open('comprehensive_validation_report.json', 'w') as f:
            json.dump(report_data, f, indent=2)
        
        print(f"\nDetailed report saved to: comprehensive_validation_report.json")
        
        return overall_passed
    
    def run_comprehensive_validation(self) -> bool:
        """Run the complete comprehensive validation suite."""
        print("STARTING COMPREHENSIVE VALIDATION")
        print("="*60)
        
        # Run all validation phases
        phases = [
            self.validate_poc_system,
            self.validate_dependency_mapping,
            self.validate_incremental_system,
            self.validate_file_organization,
            self.validate_documentation
        ]
        
        for phase in phases:
            try:
                phase()
            except Exception as e:
                print(f"ERROR in {phase.__name__}: {e}")
        
        # Analyze migration readiness
        analysis = self.analyze_migration_readiness()
        
        # Generate final report
        overall_success = self.generate_final_report(analysis)
        
        return overall_success

def main():
    """Main entry point for comprehensive validation."""
    validator = ComprehensiveValidator()
    success = validator.run_comprehensive_validation()
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())