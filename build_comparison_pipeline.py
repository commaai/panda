#!/usr/bin/env python3
"""
Build Comparison Pipeline for Panda Modular System Validation

This script provides comprehensive build comparison between legacy and modular
build systems to validate that the modular system produces equivalent outputs.

Features:
1. Binary output comparison (checksums, file sizes)
2. Build performance analysis (time, parallelism)
3. Incremental build validation
4. Regression detection
5. Detailed comparison reports
6. CI/CD integration support

Usage:
    python build_comparison_pipeline.py --all
    python build_comparison_pipeline.py --target panda_h7
    python build_comparison_pipeline.py --performance-only
    python build_comparison_pipeline.py --ci-mode
"""

import os
import sys
import time
import json
import hashlib
import subprocess
import shutil
import argparse
from pathlib import Path
from typing import Dict, List, Any
from dataclasses import dataclass, field


@dataclass
class BuildResult:
    """Results from a build operation."""
    target: str
    system: str  # 'legacy' or 'modular'
    success: bool
    build_time: float
    outputs: Dict[str, str] = field(default_factory=dict)  # filename -> hash
    file_sizes: Dict[str, int] = field(default_factory=dict)  # filename -> size
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    command: str = ""
    stdout: str = ""
    stderr: str = ""


@dataclass
class ComparisonResult:
    """Results from comparing two builds."""
    target: str
    equivalent: bool
    binary_differences: List[str] = field(default_factory=list)
    size_differences: List[str] = field(default_factory=list)
    performance_delta: Dict[str, float] = field(default_factory=dict)
    regression_detected: bool = False
    compatibility_issues: List[str] = field(default_factory=list)


class BuildComparator:
    """Main build comparison engine."""

    def __init__(self, panda_root: str = None):
        self.panda_root = Path(panda_root or os.getcwd())
        self.results_dir = self.panda_root / 'build_comparison_results'
        self.results_dir.mkdir(exist_ok=True)

        # Build configuration
        self.targets = {
            'panda_h7': {
                'legacy_script': 'SConscript',
                'modular_script': 'SConscript.modular',
                'expected_outputs': [
                    'board/obj/panda_h7/main.bin',
                    'board/obj/bootstub.panda_h7.bin',
                    'board/obj/panda_h7.bin.signed'
                ]
            },
            'panda_jungle_h7': {
                'legacy_script': 'SConscript',
                'modular_script': 'SConscript.modular',
                'expected_outputs': [
                    'board/obj/panda_jungle_h7/main.bin',
                    'board/obj/bootstub.panda_jungle_h7.bin',
                    'board/obj/panda_jungle_h7.bin.signed'
                ]
            }
        }
        # Performance tracking
        self.performance_history = []
        self.load_performance_history()

    def load_performance_history(self):
        """Load historical performance data."""
        history_file = self.results_dir / 'performance_history.json'
        if history_file.exists():
            try:
                with open(history_file, 'r') as f:
                    self.performance_history = json.load(f)
            except Exception as e:
                print(f"Warning: Could not load performance history: {e}")

    def save_performance_history(self):
        """Save performance data for trend analysis."""
        history_file = self.results_dir / 'performance_history.json'
        with open(history_file, 'w') as f:
            json.dump(self.performance_history, f, indent=2)

    def clean_build_artifacts(self):
        """Clean all build artifacts to ensure fresh builds."""
        print("Cleaning build artifacts...")

        # Clean object directories
        obj_dir = self.panda_root / 'board' / 'obj'
        if obj_dir.exists():
            shutil.rmtree(obj_dir)

        # Clean any compilation databases
        compile_db = self.panda_root / 'compile_commands.json'
        if compile_db.exists():
            compile_db.unlink()

        print("✓ Build artifacts cleaned")

    def calculate_file_hash(self, filepath: Path) -> str:
        """Calculate SHA256 hash of a file."""
        if not filepath.exists():
            return ""

        sha256_hash = hashlib.sha256()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()

    def get_file_size(self, filepath: Path) -> int:
        """Get file size in bytes."""
        return filepath.stat().st_size if filepath.exists() else 0

    def run_build(self, target: str, system: str, clean: bool = True) -> BuildResult:
        """Run a build for a specific target and system."""
        print(f"Building {target} with {system} system...")

        if clean:
            self.clean_build_artifacts()

        target_config = self.targets[target]
        script_file = target_config[f'{system}_script']

        # Prepare build command
        if system == 'legacy':
            cmd = ['scons', '-f', script_file, target]
        else:  # modular
            cmd = ['scons', '-f', script_file, f'{target}_modular']

        # Set environment variables
        env = os.environ.copy()
        env['PYTHONPATH'] = str(self.panda_root)

        result = BuildResult(
            target=target,
            system=system,
            success=False,
            build_time=0.0,
            command=' '.join(cmd)
        )

        start_time = time.time()

        try:
            # Run build command
            process = subprocess.run(
                cmd,
                cwd=self.panda_root,
                env=env,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )

            result.build_time = time.time() - start_time
            result.stdout = process.stdout
            result.stderr = process.stderr
            result.success = process.returncode == 0

            if not result.success:
                result.errors.append(f"Build failed with return code {process.returncode}")
                result.errors.append(f"stderr: {process.stderr}")
                print(f"✗ {system} build failed for {target}")
                return result

            # Collect output file information
            for output_file in target_config['expected_outputs']:
                output_path = self.panda_root / output_file
                if output_path.exists():
                    result.outputs[output_file] = self.calculate_file_hash(output_path)
                    result.file_sizes[output_file] = self.get_file_size(output_path)
                else:
                    result.errors.append(f"Expected output file not found: {output_file}")

            print(f"✓ {system} build completed for {target} ({result.build_time:.2f}s)")

        except subprocess.TimeoutExpired:
            result.errors.append("Build timed out after 5 minutes")
            result.build_time = time.time() - start_time
            print(f"✗ {system} build timed out for {target}")

        except Exception as e:
            result.errors.append(f"Build exception: {str(e)}")
            result.build_time = time.time() - start_time
            print(f"✗ {system} build failed with exception: {e}")

        return result

    def compare_builds(self, legacy_result: BuildResult, modular_result: BuildResult) -> ComparisonResult:
        """Compare results from legacy and modular builds."""
        print(f"Comparing builds for {legacy_result.target}...")

        comparison = ComparisonResult(
            target=legacy_result.target,
            equivalent=True
        )

        # Check if both builds succeeded
        if not legacy_result.success or not modular_result.success:
            comparison.equivalent = False
            if not legacy_result.success:
                comparison.compatibility_issues.append("Legacy build failed")
            if not modular_result.success:
                comparison.compatibility_issues.append("Modular build failed")
            return comparison

        # Compare binary outputs
        for filename in set(legacy_result.outputs.keys()) | set(modular_result.outputs.keys()):
            legacy_hash = legacy_result.outputs.get(filename, "MISSING")
            modular_hash = modular_result.outputs.get(filename, "MISSING")

            if legacy_hash != modular_hash:
                comparison.equivalent = False
                comparison.binary_differences.append(
                    f"{filename}: legacy={legacy_hash[:16]}..., modular={modular_hash[:16]}..."
                )

        # Compare file sizes
        for filename in set(legacy_result.file_sizes.keys()) | set(modular_result.file_sizes.keys()):
            legacy_size = legacy_result.file_sizes.get(filename, 0)
            modular_size = modular_result.file_sizes.get(filename, 0)

            if legacy_size != modular_size:
                size_diff = modular_size - legacy_size
                comparison.size_differences.append(
                    f"{filename}: legacy={legacy_size}B, modular={modular_size}B (Δ{size_diff:+d}B)"
                )

                # Large size differences might indicate compatibility issues
                if abs(size_diff) > 1024:  # More than 1KB difference
                    comparison.compatibility_issues.append(
                        f"Significant size difference in {filename}: {size_diff:+d} bytes"
                    )

        # Performance comparison
        build_time_delta = modular_result.build_time - legacy_result.build_time
        comparison.performance_delta = {
            'build_time_delta': build_time_delta,
            'legacy_time': legacy_result.build_time,
            'modular_time': modular_result.build_time,
            'speedup_factor': legacy_result.build_time / modular_result.build_time if modular_result.build_time > 0 else 0
        }

        # Detect regressions (modular significantly slower)
        if build_time_delta > legacy_result.build_time * 0.5:  # 50% slower
            comparison.regression_detected = True
            comparison.compatibility_issues.append(
                f"Performance regression: modular build {build_time_delta:.2f}s slower"
            )

        print(f"✓ Comparison completed for {legacy_result.target}")
        return comparison

    def run_incremental_build_test(self, target: str) -> Dict[str, Any]:
        """Test incremental build functionality."""
        print(f"Testing incremental builds for {target}...")

        results = {
            'target': target,
            'legacy_incremental': {},
            'modular_incremental': {},
            'incremental_equivalent': True,
            'issues': []
        }

        for system in ['legacy', 'modular']:
            print(f"  Testing {system} incremental builds...")

            # Full build
            full_result = self.run_build(target, system, clean=True)
            if not full_result.success:
                results[f'{system}_incremental']['full_build_failed'] = True
                continue

            # Touch a source file to trigger incremental build
            test_file = self.panda_root / 'board' / 'health.h'
            if test_file.exists():
                # Backup original content
                original_content = test_file.read_text()

                # Make a trivial change
                modified_content = original_content + '\n// Incremental test comment\n'
                test_file.write_text(modified_content)

                # Incremental build
                incremental_result = self.run_build(target, system, clean=False)

                # Restore original content
                test_file.write_text(original_content)

                # Record results
                results[f'{system}_incremental'] = {
                    'full_build_time': full_result.build_time,
                    'incremental_build_time': incremental_result.build_time,
                    'incremental_success': incremental_result.success,
                    'speedup': full_result.build_time / incremental_result.build_time if incremental_result.build_time > 0 else 0
                }

                # Validate incremental build is faster
                if incremental_result.build_time >= full_result.build_time:
                    results['issues'].append(f"{system} incremental build not faster than full build")
                    results['incremental_equivalent'] = False

        print(f"✓ Incremental build test completed for {target}")
        return results

    def run_parallel_build_test(self, target: str) -> Dict[str, Any]:
        """Test parallel build performance."""
        print(f"Testing parallel builds for {target}...")

        results = {
            'target': target,
            'sequential_times': {},
            'parallel_times': {},
            'parallel_efficiency': {}
        }

        for system in ['legacy', 'modular']:
            # Sequential build (j1)
            os.environ['SCONS_FLAGS'] = '-j1'
            sequential_result = self.run_build(target, system, clean=True)

            # Parallel build (j4)
            os.environ['SCONS_FLAGS'] = '-j4'
            parallel_result = self.run_build(target, system, clean=True)

            # Clean up environment
            os.environ.pop('SCONS_FLAGS', None)

            if sequential_result.success and parallel_result.success:
                results['sequential_times'][system] = sequential_result.build_time
                results['parallel_times'][system] = parallel_result.build_time
                results['parallel_efficiency'][system] = sequential_result.build_time / parallel_result.build_time

        print(f"✓ Parallel build test completed for {target}")
        return results

    def analyze_performance_trends(self) -> Dict[str, Any]:
        """Analyze performance trends over time."""
        if len(self.performance_history) < 2:
            return {'insufficient_data': True}

        # Calculate trends for different metrics
        trends = {
            'build_time_trend': [],
            'binary_size_trend': [],
            'regression_count': 0
        }

        # Analyze last 10 runs
        recent_history = self.performance_history[-10:]

        for i in range(1, len(recent_history)):
            prev = recent_history[i-1]
            curr = recent_history[i]

            # Build time trend
            if 'build_time' in prev and 'build_time' in curr:
                time_change = curr['build_time'] - prev['build_time']
                trends['build_time_trend'].append(time_change)

                if time_change > prev['build_time'] * 0.2:  # 20% regression
                    trends['regression_count'] += 1

        return trends

    def generate_comparison_report(self, results: List[ComparisonResult],
                                 incremental_results: List[Dict],
                                 parallel_results: List[Dict]) -> Dict[str, Any]:
        """Generate comprehensive comparison report."""
        timestamp = time.time()

        # Overall status
        all_equivalent = all(r.equivalent for r in results)
        total_regressions = sum(1 for r in results if r.regression_detected)

        # Performance summary
        performance_summary = {}
        for result in results:
            if result.performance_delta:
                performance_summary[result.target] = {
                    'speedup': result.performance_delta.get('speedup_factor', 0),
                    'time_delta': result.performance_delta.get('build_time_delta', 0)
                }

        # Compatibility issues
        all_issues = []
        for result in results:
            all_issues.extend(result.compatibility_issues)

        report = {
            'timestamp': timestamp,
            'overall_status': {
                'all_equivalent': all_equivalent,
                'total_targets': len(results),
                'equivalent_targets': sum(1 for r in results if r.equivalent),
                'total_regressions': total_regressions,
                'ready_for_migration': all_equivalent and total_regressions == 0
            },
            'detailed_results': [
                {
                    'target': r.target,
                    'equivalent': r.equivalent,
                    'binary_differences': r.binary_differences,
                    'size_differences': r.size_differences,
                    'performance': r.performance_delta,
                    'regression_detected': r.regression_detected,
                    'compatibility_issues': r.compatibility_issues
                }
                for r in results
            ],
            'performance_summary': performance_summary,
            'incremental_build_results': incremental_results,
            'parallel_build_results': parallel_results,
            'all_compatibility_issues': all_issues,
            'recommendations': self.generate_recommendations(results, all_issues)
        }

        # Save report
        report_file = self.results_dir / f'build_comparison_report_{int(timestamp)}.json'
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)

        # Save as latest
        latest_file = self.results_dir / 'latest_comparison_report.json'
        with open(latest_file, 'w') as f:
            json.dump(report, f, indent=2)

        # Update performance history
        perf_entry = {
            'timestamp': timestamp,
            'build_time': sum(r.performance_delta.get('modular_time', 0) for r in results),
            'equivalent': all_equivalent,
            'regression_count': total_regressions
        }
        self.performance_history.append(perf_entry)
        self.save_performance_history()

        return report

    def generate_recommendations(self, results: List[ComparisonResult], issues: List[str]) -> List[str]:
        """Generate actionable recommendations based on results."""
        recommendations = []

        if all(r.equivalent for r in results):
            recommendations.append("✓ All targets produce equivalent outputs - migration can proceed")
            recommendations.append("Consider setting up automated validation in CI/CD pipeline")
        else:
            recommendations.append("⚠ Binary differences detected - investigate before migration")
            recommendations.append("Run detailed binary analysis on differing outputs")

        if any(r.regression_detected for r in results):
            recommendations.append("⚠ Performance regressions detected - optimization needed")
            recommendations.append("Profile modular build system for bottlenecks")

        if issues:
            recommendations.append("⚠ Compatibility issues found - review and address")
            recommendations.append("Create rollback plan before proceeding with migration")
        else:
            recommendations.append("✓ No compatibility issues detected")

        # Performance recommendations
        avg_speedup = sum(r.performance_delta.get('speedup_factor', 1) for r in results) / len(results)
        if avg_speedup > 1.1:
            recommendations.append(f"✓ Modular system shows {avg_speedup:.1f}x average speedup")
        elif avg_speedup < 0.9:
            recommendations.append(f"⚠ Modular system {1/avg_speedup:.1f}x slower - investigate")

        return recommendations

    def print_summary_report(self, report: Dict[str, Any]):
        """Print a human-readable summary of the comparison."""
        print(f"\n{'='*80}")
        print("BUILD COMPARISON PIPELINE SUMMARY")
        print(f"{'='*80}")

        status = report['overall_status']
        print(f"\nOVERALL STATUS:")
        print(f"  Ready for migration: {'✓ YES' if status['ready_for_migration'] else '✗ NO'}")
        print(f"  Equivalent outputs: {status['equivalent_targets']}/{status['total_targets']} targets")
        print(f"  Performance regressions: {status['total_regressions']}")

        print(f"\nTARGET RESULTS:")
        for result in report['detailed_results']:
            equiv_status = "✓" if result['equivalent'] else "✗"
            regression_status = " (REGRESSION)" if result['regression_detected'] else ""
            print(f"  {equiv_status} {result['target']}{regression_status}")

            if result['performance'].get('speedup_factor'):
                speedup = result['performance']['speedup_factor']
                if speedup > 1:
                    print(f"    Performance: {speedup:.2f}x faster")
                else:
                    print(f"    Performance: {1/speedup:.2f}x slower")

        if report['all_compatibility_issues']:
            print(f"\nCOMPATIBILITY ISSUES:")
            for issue in report['all_compatibility_issues'][:5]:  # Show first 5
                print(f"  ⚠ {issue}")
            if len(report['all_compatibility_issues']) > 5:
                print(f"  ... and {len(report['all_compatibility_issues']) - 5} more")

        print(f"\nRECOMMENDATIONS:")
        for rec in report['recommendations']:
            print(f"  {rec}")

        print(f"\nDetailed report saved to: {self.results_dir / 'latest_comparison_report.json'}")

    def run_comprehensive_comparison(self, targets: List[str] = None,
                                   include_performance_tests: bool = True) -> Dict[str, Any]:
        """Run comprehensive build comparison."""
        print("Starting comprehensive build comparison...")

        if targets is None:
            targets = list(self.targets.keys())

        comparison_results = []
        incremental_results = []
        parallel_results = []

        # Main comparison for each target
        for target in targets:
            try:
                # Build with both systems
                legacy_result = self.run_build(target, 'legacy')
                modular_result = self.run_build(target, 'modular')

                # Compare results
                comparison = self.compare_builds(legacy_result, modular_result)
                comparison_results.append(comparison)

                # Performance tests
                if include_performance_tests:
                    incremental_test = self.run_incremental_build_test(target)
                    incremental_results.append(incremental_test)

                    parallel_test = self.run_parallel_build_test(target)
                    parallel_results.append(parallel_test)

            except Exception as e:
                print(f"Error testing {target}: {e}")
                # Create a failed comparison result
                failed_comparison = ComparisonResult(
                    target=target,
                    equivalent=False,
                    compatibility_issues=[f"Test failed: {str(e)}"]
                )
                comparison_results.append(failed_comparison)

        # Generate comprehensive report
        report = self.generate_comparison_report(comparison_results, incremental_results, parallel_results)
        self.print_summary_report(report)

        return report


def main():
    """Main entry point for the build comparison pipeline."""
    parser = argparse.ArgumentParser(description='Panda Build Comparison Pipeline')
    parser.add_argument('--target', help='Specific target to test (panda_h7, panda_jungle_h7)')
    parser.add_argument('--all', action='store_true', help='Test all targets')
    parser.add_argument('--performance-only', action='store_true', help='Run only performance tests')
    parser.add_argument('--ci-mode', action='store_true', help='CI mode - minimal output, exit codes')
    parser.add_argument('--clean', action='store_true', help='Clean before builds', default=True)

    args = parser.parse_args()

    comparator = BuildComparator()

    # Determine targets to test
    targets = []
    if args.target:
        if args.target not in comparator.targets:
            print(f"Error: Unknown target {args.target}")
            print(f"Available targets: {list(comparator.targets.keys())}")
            return 1
        targets = [args.target]
    elif args.all:
        targets = list(comparator.targets.keys())
    else:
        targets = ['panda_h7']  # Default

    try:
        # Run comprehensive comparison
        report = comparator.run_comprehensive_comparison(
            targets=targets,
            include_performance_tests=not args.performance_only
        )

        # Exit code for CI
        if args.ci_mode:
            success = report['overall_status']['ready_for_migration']
            return 0 if success else 1

        return 0

    except KeyboardInterrupt:
        print("\nComparison interrupted by user")
        return 1
    except Exception as e:
        print(f"Error during comparison: {e}")
        if not args.ci_mode:
            import traceback
            traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
