#!/usr/bin/env python3
"""
Migration Orchestrator for Panda Modular Build System

This master script orchestrates the entire migration validation process,
providing a single entry point for all validation activities and ensuring
proper coordination between all validation tools.

Features:
1. Orchestrates all validation tools
2. Manages migration workflow
3. Provides unified reporting
4. Handles error recovery
5. Tracks migration progress
6. Generates stakeholder reports
7. Manages deployment readiness

Usage:
    python migration_orchestrator.py --validate-all
    python migration_orchestrator.py --migration-readiness
    python migration_orchestrator.py --performance-only
    python migration_orchestrator.py --safety-check
    python migration_orchestrator.py --generate-reports
"""

import os
import sys
import json
import time
import subprocess
import argparse
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field, asdict
from datetime import datetime
import concurrent.futures
import threading


@dataclass
class OrchestrationResult:
    """Results from orchestrated validation."""
    timestamp: float
    orchestration_id: str
    phase: str
    success: bool
    duration: float
    tool_results: Dict[str, Any] = field(default_factory=dict)
    summary: Dict[str, Any] = field(default_factory=dict)
    recommendations: List[str] = field(default_factory=list)
    next_steps: List[str] = field(default_factory=list)


class MigrationOrchestrator:
    """Master orchestrator for migration validation."""
    
    def __init__(self, panda_root: str = None):
        self.panda_root = Path(panda_root or os.getcwd())
        self.orchestration_dir = self.panda_root / 'migration_orchestration'
        self.orchestration_dir.mkdir(exist_ok=True)
        
        # Available validation tools
        self.validation_tools = {
            'build_comparison': {
                'script': 'build_comparison_pipeline.py',
                'description': 'Build comparison between legacy and modular',
                'category': 'functional',
                'critical': True
            },
            'ci_validation': {
                'script': 'ci_validation_pipeline.py',
                'description': 'CI/CD validation pipeline',
                'category': 'functional',
                'critical': True
            },
            'performance_analysis': {
                'script': 'performance_analysis_suite.py',
                'description': 'Performance analysis and benchmarking',
                'category': 'performance',
                'critical': False
            },
            'safety_system': {
                'script': 'rollback_safety_system.py',
                'description': 'Rollback and safety mechanisms',
                'category': 'safety',
                'critical': True
            },
            'validation_framework': {
                'script': 'validation_framework.py',
                'description': 'Comprehensive validation framework',
                'category': 'comprehensive',
                'critical': True
            }
        }
        
        # Orchestration state
        self.state_file = self.orchestration_dir / 'orchestration_state.json'
        self.state = self._load_state()
        
        # Thread safety
        self._lock = threading.Lock()
    
    def _load_state(self) -> Dict[str, Any]:
        """Load orchestration state."""
        if self.state_file.exists():
            try:
                with open(self.state_file, 'r') as f:
                    return json.load(f)
            except Exception as e:
                print(f"Warning: Could not load state: {e}")
        
        return {
            'last_run': None,
            'migration_phase': 'initial',
            'tool_status': {},
            'validation_history': []
        }
    
    def _save_state(self):
        """Save orchestration state."""
        with self._lock:
            with open(self.state_file, 'w') as f:
                json.dump(self.state, f, indent=2)
    
    def run_comprehensive_validation(self) -> OrchestrationResult:
        """Run comprehensive validation across all tools."""
        print("🚀 Starting comprehensive migration validation...")
        
        orchestration_id = f"comprehensive_{int(time.time())}"
        start_time = time.time()
        
        result = OrchestrationResult(
            timestamp=start_time,
            orchestration_id=orchestration_id,
            phase="comprehensive_validation",
            success=False,
            duration=0.0
        )
        
        # Phase 1: Safety preparation
        print("\n📋 Phase 1: Safety Preparation")
        safety_result = self._run_safety_preparation()
        result.tool_results['safety_preparation'] = safety_result
        
        if not safety_result.get('success', False):
            print("❌ Safety preparation failed - aborting")
            result.duration = time.time() - start_time
            self._save_orchestration_result(result)
            return result
        
        # Phase 2: Functional validation
        print("\n🔧 Phase 2: Functional Validation")
        functional_results = self._run_functional_validation()
        result.tool_results['functional'] = functional_results
        
        # Phase 3: Performance validation
        print("\n⚡ Phase 3: Performance Validation")
        performance_results = self._run_performance_validation()
        result.tool_results['performance'] = performance_results
        
        # Phase 4: Comprehensive framework validation
        print("\n🏗️ Phase 4: Comprehensive Framework Validation")
        framework_results = self._run_framework_validation()
        result.tool_results['framework'] = framework_results
        
        # Phase 5: Results synthesis
        print("\n📊 Phase 5: Results Synthesis")
        result.summary = self._synthesize_results(result.tool_results)
        result.success = self._determine_overall_success(result.tool_results)
        result.recommendations = self._generate_master_recommendations(result.tool_results)
        result.next_steps = self._generate_next_steps(result.tool_results, result.success)
        
        result.duration = time.time() - start_time
        
        # Update state
        self._update_orchestration_state(result)
        
        # Save detailed results
        self._save_orchestration_result(result)
        
        # Generate final report
        self._generate_master_report(result)
        
        # Print summary
        self._print_orchestration_summary(result)
        
        return result
    
    def _run_safety_preparation(self) -> Dict[str, Any]:
        """Run safety preparation phase."""
        results = {
            'success': False,
            'snapshot_created': False,
            'safety_checks_passed': False,
            'rollback_tested': False
        }
        
        try:
            # Create system snapshot
            print("  📸 Creating system snapshot...")
            snapshot_result = subprocess.run(
                [sys.executable, 'rollback_safety_system.py', 
                 '--create-snapshot', 'Pre-validation snapshot', '--stage', 'validation'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=300
            )
            
            results['snapshot_created'] = snapshot_result.returncode == 0
            if results['snapshot_created']:
                print("    ✅ System snapshot created")
            else:
                print("    ❌ Failed to create system snapshot")
                return results
            
            # Run safety checks
            print("  🛡️ Running safety checks...")
            safety_result = subprocess.run(
                [sys.executable, 'rollback_safety_system.py', '--safety-checks'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=600
            )
            
            results['safety_checks_passed'] = safety_result.returncode == 0
            if results['safety_checks_passed']:
                print("    ✅ Safety checks passed")
            else:
                print("    ❌ Safety checks failed")
                print(f"    Details: {safety_result.stderr}")
            
            # Test rollback mechanism (dry run)
            print("  🔄 Testing rollback mechanism...")
            # For safety, we just verify the safety system is responsive
            status_result = subprocess.run(
                [sys.executable, 'rollback_safety_system.py', '--status'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=60
            )
            
            results['rollback_tested'] = status_result.returncode == 0
            if results['rollback_tested']:
                print("    ✅ Rollback mechanism responsive")
            else:
                print("    ❌ Rollback mechanism not responsive")
            
            results['success'] = all([
                results['snapshot_created'],
                results['safety_checks_passed'],
                results['rollback_tested']
            ])
            
        except Exception as e:
            print(f"    ❌ Safety preparation error: {e}")
            results['error'] = str(e)
        
        return results
    
    def _run_functional_validation(self) -> Dict[str, Any]:
        """Run functional validation phase."""
        results = {
            'build_comparison': {},
            'ci_validation': {},
            'overall_success': False
        }
        
        # Run build comparison
        print("  🔄 Running build comparison...")
        try:
            comparison_result = subprocess.run(
                [sys.executable, 'build_comparison_pipeline.py', '--all'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=1200  # 20 minutes
            )
            
            results['build_comparison'] = {
                'success': comparison_result.returncode == 0,
                'stdout': comparison_result.stdout,
                'stderr': comparison_result.stderr
            }
            
            if results['build_comparison']['success']:
                print("    ✅ Build comparison passed")
            else:
                print("    ❌ Build comparison failed")
        
        except Exception as e:
            print(f"    ❌ Build comparison error: {e}")
            results['build_comparison'] = {'success': False, 'error': str(e)}
        
        # Run CI validation
        print("  🏗️ Running CI validation...")
        try:
            ci_result = subprocess.run(
                [sys.executable, 'ci_validation_pipeline.py', '--targets', 'panda_h7'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=1200  # 20 minutes
            )
            
            results['ci_validation'] = {
                'success': ci_result.returncode == 0,
                'stdout': ci_result.stdout,
                'stderr': ci_result.stderr
            }
            
            if results['ci_validation']['success']:
                print("    ✅ CI validation passed")
            else:
                print("    ❌ CI validation failed")
        
        except Exception as e:
            print(f"    ❌ CI validation error: {e}")
            results['ci_validation'] = {'success': False, 'error': str(e)}
        
        results['overall_success'] = (
            results['build_comparison'].get('success', False) and
            results['ci_validation'].get('success', False)
        )
        
        return results
    
    def _run_performance_validation(self) -> Dict[str, Any]:
        """Run performance validation phase."""
        results = {
            'performance_analysis': {},
            'overall_success': False
        }
        
        print("  ⚡ Running performance analysis...")
        try:
            perf_result = subprocess.run(
                [sys.executable, 'performance_analysis_suite.py', '--full-analysis'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=1800  # 30 minutes
            )
            
            results['performance_analysis'] = {
                'success': perf_result.returncode == 0,
                'stdout': perf_result.stdout,
                'stderr': perf_result.stderr
            }
            
            # Parse performance results if available
            perf_report_file = self.panda_root / 'performance_analysis_results' / 'latest_performance_report.json'
            if perf_report_file.exists():
                try:
                    with open(perf_report_file, 'r') as f:
                        perf_data = json.load(f)
                    results['performance_analysis']['data'] = perf_data
                except Exception:
                    pass
            
            if results['performance_analysis']['success']:
                print("    ✅ Performance analysis completed")
            else:
                print("    ❌ Performance analysis failed")
        
        except Exception as e:
            print(f"    ❌ Performance analysis error: {e}")
            results['performance_analysis'] = {'success': False, 'error': str(e)}
        
        results['overall_success'] = results['performance_analysis'].get('success', False)
        
        return results
    
    def _run_framework_validation(self) -> Dict[str, Any]:
        """Run comprehensive framework validation."""
        results = {
            'validation_framework': {},
            'overall_success': False
        }
        
        print("  🏗️ Running validation framework...")
        try:
            framework_result = subprocess.run(
                [sys.executable, 'validation_framework.py', '--comprehensive'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=1800  # 30 minutes
            )
            
            results['validation_framework'] = {
                'success': framework_result.returncode == 0,
                'stdout': framework_result.stdout,
                'stderr': framework_result.stderr
            }
            
            # Parse framework results if available
            framework_report_file = self.panda_root / 'validation_reports' / 'latest_validation_report.json'
            if framework_report_file.exists():
                try:
                    with open(framework_report_file, 'r') as f:
                        framework_data = json.load(f)
                    results['validation_framework']['data'] = framework_data
                except Exception:
                    pass
            
            if results['validation_framework']['success']:
                print("    ✅ Validation framework completed")
            else:
                print("    ❌ Validation framework failed")
        
        except Exception as e:
            print(f"    ❌ Validation framework error: {e}")
            results['validation_framework'] = {'success': False, 'error': str(e)}
        
        results['overall_success'] = results['validation_framework'].get('success', False)
        
        return results
    
    def _synthesize_results(self, tool_results: Dict[str, Any]) -> Dict[str, Any]:
        """Synthesize results from all tools."""
        synthesis = {
            'overall_score': 0.0,
            'functional_ready': False,
            'performance_acceptable': False,
            'safety_validated': False,
            'migration_ready': False,
            'critical_issues': [],
            'success_metrics': {}
        }
        
        # Analyze functional readiness
        functional = tool_results.get('functional', {})
        build_comparison_ok = functional.get('build_comparison', {}).get('success', False)
        ci_validation_ok = functional.get('ci_validation', {}).get('success', False)
        synthesis['functional_ready'] = build_comparison_ok and ci_validation_ok
        
        # Analyze performance
        performance = tool_results.get('performance', {})
        perf_analysis_ok = performance.get('performance_analysis', {}).get('success', False)
        synthesis['performance_acceptable'] = perf_analysis_ok
        
        # Analyze safety
        safety = tool_results.get('safety_preparation', {})
        safety_checks_ok = safety.get('safety_checks_passed', False)
        rollback_ok = safety.get('rollback_tested', False)
        synthesis['safety_validated'] = safety_checks_ok and rollback_ok
        
        # Analyze framework results
        framework = tool_results.get('framework', {})
        framework_data = framework.get('validation_framework', {}).get('data', {})
        if framework_data:
            synthesis['overall_score'] = framework_data.get('overall_score', 0.0)
            synthesis['migration_ready'] = framework_data.get('migration_ready', False)
        
        # Determine overall migration readiness
        if not synthesis['migration_ready']:
            synthesis['migration_ready'] = all([
                synthesis['functional_ready'],
                synthesis['performance_acceptable'],
                synthesis['safety_validated']
            ])
        
        # Collect critical issues
        for phase, results in tool_results.items():
            if isinstance(results, dict):
                for tool, tool_result in results.items():
                    if isinstance(tool_result, dict) and not tool_result.get('success', True):
                        if tool_result.get('stderr'):
                            synthesis['critical_issues'].append(f"{phase}/{tool}: {tool_result['stderr'][:100]}...")
                        elif tool_result.get('error'):
                            synthesis['critical_issues'].append(f"{phase}/{tool}: {tool_result['error']}")
        
        # Success metrics
        synthesis['success_metrics'] = {
            'functional_tests_passed': synthesis['functional_ready'],
            'performance_tests_passed': synthesis['performance_acceptable'],
            'safety_tests_passed': synthesis['safety_validated'],
            'overall_score': synthesis['overall_score']
        }
        
        return synthesis
    
    def _determine_overall_success(self, tool_results: Dict[str, Any]) -> bool:
        """Determine if overall validation was successful."""
        # Must pass all critical phases
        safety_ok = tool_results.get('safety_preparation', {}).get('success', False)
        functional_ok = tool_results.get('functional', {}).get('overall_success', False)
        
        # Performance is important but not blocking
        performance_ok = tool_results.get('performance', {}).get('overall_success', False)
        
        # Framework validation should pass
        framework_ok = tool_results.get('framework', {}).get('overall_success', False)
        
        return safety_ok and functional_ok and (performance_ok or framework_ok)
    
    def _generate_master_recommendations(self, tool_results: Dict[str, Any]) -> List[str]:
        """Generate master recommendations from all tool results."""
        recommendations = []
        
        # Safety recommendations
        safety = tool_results.get('safety_preparation', {})
        if not safety.get('success', False):
            recommendations.append("🚨 Critical: Fix safety system issues before proceeding")
            if not safety.get('snapshot_created', False):
                recommendations.append("📸 Create system snapshot before any migration activities")
            if not safety.get('safety_checks_passed', False):
                recommendations.append("🛡️ Address safety check failures")
        
        # Functional recommendations
        functional = tool_results.get('functional', {})
        if not functional.get('overall_success', False):
            recommendations.append("🔧 Critical: Address functional validation failures")
            
            if not functional.get('build_comparison', {}).get('success', False):
                recommendations.append("⚖️ Fix binary equivalence issues between legacy and modular builds")
            
            if not functional.get('ci_validation', {}).get('success', False):
                recommendations.append("🏗️ Fix CI validation pipeline failures")
        
        # Performance recommendations
        performance = tool_results.get('performance', {})
        if not performance.get('overall_success', False):
            recommendations.append("⚡ Investigate and fix performance issues")
        
        # Framework recommendations
        framework = tool_results.get('framework', {})
        framework_data = framework.get('validation_framework', {}).get('data', {})
        if framework_data and 'recommendations' in framework_data:
            recommendations.extend(framework_data['recommendations'][:3])  # Top 3
        
        # Migration readiness recommendations
        synthesis = self._synthesize_results(tool_results)
        if synthesis['migration_ready']:
            recommendations.append("✅ System is ready for migration - proceed with deployment")
            recommendations.append("📋 Implement monitoring and rollback procedures")
            recommendations.append("🎯 Consider phased rollout starting with non-critical targets")
        else:
            recommendations.append("⚠️ System not ready for migration - address critical issues first")
        
        return recommendations
    
    def _generate_next_steps(self, tool_results: Dict[str, Any], success: bool) -> List[str]:
        """Generate next steps based on validation results."""
        if success:
            return [
                "1. Review and approve migration plan with stakeholders",
                "2. Schedule migration window with appropriate rollback time",
                "3. Set up monitoring and alerting for migration",
                "4. Execute phased migration starting with panda_h7",
                "5. Monitor system behavior and performance post-migration"
            ]
        else:
            return [
                "1. Address all critical validation failures",
                "2. Re-run comprehensive validation",
                "3. Review and update migration timeline", 
                "4. Consider additional testing and validation",
                "5. Reassess migration readiness after fixes"
            ]
    
    def _update_orchestration_state(self, result: OrchestrationResult):
        """Update orchestration state with latest results."""
        self.state['last_run'] = result.timestamp
        self.state['migration_phase'] = 'validated' if result.success else 'validation_failed'
        
        # Update tool status
        for tool_name, tool_info in self.validation_tools.items():
            category = tool_info['category']
            if category in result.tool_results:
                self.state['tool_status'][tool_name] = {
                    'last_run': result.timestamp,
                    'success': result.tool_results[category].get('overall_success', False)
                }
        
        # Add to history
        history_entry = {
            'timestamp': result.timestamp,
            'orchestration_id': result.orchestration_id,
            'success': result.success,
            'duration': result.duration,
            'summary_score': result.summary.get('overall_score', 0.0)
        }
        
        self.state['validation_history'].append(history_entry)
        
        # Keep only last 10 entries
        self.state['validation_history'] = self.state['validation_history'][-10:]
        
        self._save_state()
    
    def _save_orchestration_result(self, result: OrchestrationResult):
        """Save orchestration result to file."""
        result_file = self.orchestration_dir / f'{result.orchestration_id}.json'
        with open(result_file, 'w') as f:
            json.dump(asdict(result), f, indent=2)
        
        # Save as latest
        latest_file = self.orchestration_dir / 'latest_orchestration_result.json'
        with open(latest_file, 'w') as f:
            json.dump(asdict(result), f, indent=2)
    
    def _generate_master_report(self, result: OrchestrationResult):
        """Generate master stakeholder report."""
        report = {
            'executive_summary': {
                'validation_date': datetime.fromtimestamp(result.timestamp).strftime('%Y-%m-%d %H:%M:%S'),
                'migration_ready': result.summary.get('migration_ready', False),
                'overall_score': result.summary.get('overall_score', 0.0),
                'validation_duration': f"{result.duration / 60:.1f} minutes",
                'critical_issues_count': len(result.summary.get('critical_issues', []))
            },
            'validation_phases': {
                'safety_preparation': result.tool_results.get('safety_preparation', {}).get('success', False),
                'functional_validation': result.tool_results.get('functional', {}).get('overall_success', False),
                'performance_validation': result.tool_results.get('performance', {}).get('overall_success', False),
                'framework_validation': result.tool_results.get('framework', {}).get('overall_success', False)
            },
            'recommendations': result.recommendations,
            'next_steps': result.next_steps,
            'detailed_results': result.tool_results
        }
        
        report_file = self.orchestration_dir / f'{result.orchestration_id}_master_report.json'
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"📊 Master report generated: {report_file}")
    
    def _print_orchestration_summary(self, result: OrchestrationResult):
        """Print comprehensive orchestration summary."""
        print(f"\n{'='*80}")
        print("🎯 MIGRATION VALIDATION ORCHESTRATION SUMMARY")
        print(f"{'='*80}")
        
        # Overall status
        status_icon = "✅" if result.success else "❌"
        ready_status = "READY" if result.summary.get('migration_ready', False) else "NOT READY"
        
        print(f"\n{status_icon} OVERALL STATUS: {ready_status}")
        print(f"📊 Overall Score: {result.summary.get('overall_score', 0.0):.1f}/100")
        print(f"⏱️ Validation Duration: {result.duration / 60:.1f} minutes")
        
        # Phase results
        print(f"\n📋 VALIDATION PHASES:")
        phases = {
            'Safety Preparation': result.tool_results.get('safety_preparation', {}).get('success', False),
            'Functional Validation': result.tool_results.get('functional', {}).get('overall_success', False),
            'Performance Validation': result.tool_results.get('performance', {}).get('overall_success', False),
            'Framework Validation': result.tool_results.get('framework', {}).get('overall_success', False)
        }
        
        for phase_name, phase_success in phases.items():
            icon = "✅" if phase_success else "❌"
            print(f"  {icon} {phase_name}")
        
        # Critical issues
        critical_issues = result.summary.get('critical_issues', [])
        if critical_issues:
            print(f"\n🚨 CRITICAL ISSUES ({len(critical_issues)}):")
            for issue in critical_issues[:3]:  # Show first 3
                print(f"  ❌ {issue}")
            if len(critical_issues) > 3:
                print(f"  ... and {len(critical_issues) - 3} more issues")
        
        # Top recommendations
        print(f"\n💡 KEY RECOMMENDATIONS:")
        for i, rec in enumerate(result.recommendations[:5], 1):
            print(f"  {i}. {rec}")
        
        # Next steps
        print(f"\n🎯 NEXT STEPS:")
        for step in result.next_steps:
            print(f"  📌 {step}")
        
        print(f"\n📁 Detailed results: {self.orchestration_dir}")
        print(f"📊 All reports available in respective tool directories")
        
        if result.success:
            print(f"\n🎉 VALIDATION SUCCESSFUL - MIGRATION CAN PROCEED")
        else:
            print(f"\n⚠️ VALIDATION INCOMPLETE - ADDRESS ISSUES BEFORE MIGRATION")


def main():
    """Main entry point for migration orchestrator."""
    parser = argparse.ArgumentParser(description='Panda Migration Orchestrator')
    parser.add_argument('--validate-all', action='store_true',
                       help='Run comprehensive validation')
    parser.add_argument('--migration-readiness', action='store_true',
                       help='Check migration readiness')
    parser.add_argument('--performance-only', action='store_true',
                       help='Run only performance validation')
    parser.add_argument('--safety-check', action='store_true',
                       help='Run only safety checks')
    parser.add_argument('--generate-reports', action='store_true',
                       help='Generate stakeholder reports')
    parser.add_argument('--status', action='store_true',
                       help='Show orchestration status')
    
    args = parser.parse_args()
    
    orchestrator = MigrationOrchestrator()
    
    if args.validate_all or args.migration_readiness or not any(vars(args).values()):
        # Run comprehensive validation
        result = orchestrator.run_comprehensive_validation()
        return 0 if result.success else 1
    
    elif args.performance_only:
        # Run only performance validation
        print("🚀 Running performance-only validation...")
        perf_results = orchestrator._run_performance_validation()
        success = perf_results.get('overall_success', False)
        print(f"Performance validation: {'✅ PASSED' if success else '❌ FAILED'}")
        return 0 if success else 1
    
    elif args.safety_check:
        # Run only safety checks
        print("🚀 Running safety check...")
        safety_results = orchestrator._run_safety_preparation()
        success = safety_results.get('success', False)
        print(f"Safety check: {'✅ PASSED' if success else '❌ FAILED'}")
        return 0 if success else 1
    
    elif args.generate_reports:
        # Generate reports from last run
        latest_file = orchestrator.orchestration_dir / 'latest_orchestration_result.json'
        if latest_file.exists():
            with open(latest_file, 'r') as f:
                result_data = json.load(f)
            result = OrchestrationResult(**result_data)
            orchestrator._generate_master_report(result)
            print("📊 Reports generated successfully")
            return 0
        else:
            print("❌ No previous results found - run validation first")
            return 1
    
    elif args.status:
        # Show status
        state = orchestrator.state
        print(f"Migration phase: {state.get('migration_phase', 'unknown')}")
        if state.get('last_run'):
            last_run = datetime.fromtimestamp(state['last_run']).strftime('%Y-%m-%d %H:%M:%S')
            print(f"Last run: {last_run}")
        
        print(f"Tool status:")
        for tool, status in state.get('tool_status', {}).items():
            success = "✅" if status.get('success', False) else "❌"
            print(f"  {success} {tool}")
        
        return 0
    
    return 0


if __name__ == "__main__":
    sys.exit(main())