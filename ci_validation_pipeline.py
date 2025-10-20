#!/usr/bin/env python3
"""
CI/CD Validation Pipeline for Panda Modular Build System

This script provides automated validation that can be integrated into CI/CD
pipelines to ensure the modular build system remains compatible and performs
as expected.

Features:
1. Automated regression testing
2. Performance benchmarking
3. Binary compatibility validation
4. Incremental build verification
5. Parallel execution for speed
6. Detailed CI reports
7. Build artifact archival
8. Notification system integration

Environment Variables:
    CI_MODE: Set to enable CI-specific behavior
    BUILD_TIMEOUT: Build timeout in seconds (default: 300)
    PARALLEL_JOBS: Number of parallel jobs (default: 4)
    ARCHIVE_ARTIFACTS: Set to archive build artifacts
    NOTIFICATION_WEBHOOK: Webhook URL for notifications
"""

import os
import sys
import json
import time
import hashlib
import subprocess
from pathlib import Path
from typing import Dict, List, Any
from dataclasses import dataclass, asdict
from concurrent.futures import ThreadPoolExecutor, as_completed
import zipfile


@dataclass
class CIValidationConfig:
    """Configuration for CI validation."""
    enable_performance_tests: bool = True
    enable_regression_tests: bool = True
    enable_incremental_tests: bool = True
    build_timeout: int = 300  # 5 minutes
    parallel_jobs: int = 4
    archive_artifacts: bool = False
    notification_webhook: Optional[str] = None
    fail_on_performance_regression: bool = True
    fail_on_size_increase: bool = True
    max_size_increase_percent: float = 5.0  # 5% max increase
    max_performance_regression_percent: float = 20.0  # 20% max regression


@dataclass
class CITestResult:
    """Results from a CI test."""
    test_name: str
    status: str  # 'PASS', 'FAIL', 'SKIP', 'ERROR'
    duration: float
    message: str = ""
    details: Dict[str, Any] = None
    artifacts: List[str] = None


class CIValidationPipeline:
    """Main CI validation pipeline."""

    def __init__(self, config: CIValidationConfig = None):
        self.config = config or CIValidationConfig()
        self.panda_root = Path(os.getcwd())

        # Load config from environment
        self._load_env_config()

        # Initialize results tracking
        self.test_results: List[CITestResult] = []
        self.artifacts_dir = self.panda_root / 'ci_artifacts'
        self.artifacts_dir.mkdir(exist_ok=True)

        # Thread safety for parallel execution
        self.results_lock = threading.Lock()

        # Load baseline data for comparison
        self.baseline_data = self._load_baseline_data()

    def _load_env_config(self):
        """Load configuration from environment variables."""
        if os.getenv('CI_MODE'):
            # CI-specific optimizations
            self.config.parallel_jobs = int(os.getenv('PARALLEL_JOBS', '4'))
            self.config.build_timeout = int(os.getenv('BUILD_TIMEOUT', '300'))
            self.config.archive_artifacts = bool(os.getenv('ARCHIVE_ARTIFACTS'))
            self.config.notification_webhook = os.getenv('NOTIFICATION_WEBHOOK')

    def _load_baseline_data(self) -> Dict[str, Any]:
        """Load baseline performance and size data."""
        baseline_file = self.panda_root / 'ci_baseline.json'
        if baseline_file.exists():
            try:
                with open(baseline_file, 'r') as f:
                    return json.load(f)
            except Exception as e:
                self._log(f"Warning: Could not load baseline data: {e}")
        return {}

    def _save_baseline_data(self, data: Dict[str, Any]):
        """Save new baseline data."""
        baseline_file = self.panda_root / 'ci_baseline.json'
        with open(baseline_file, 'w') as f:
            json.dump(data, f, indent=2)

    def _log(self, message: str, level: str = "INFO"):
        """Thread-safe logging."""
        timestamp = time.strftime("%H:%M:%S")
        print(f"[{timestamp}] {level}: {message}")

    def _add_test_result(self, result: CITestResult):
        """Thread-safe way to add test results."""
        with self.results_lock:
            self.test_results.append(result)

    def _run_command(self, cmd: List[str], timeout: int = None, cwd: Path = None) -> subprocess.CompletedProcess:
        """Run a command with proper error handling."""
        timeout = timeout or self.config.build_timeout
        cwd = cwd or self.panda_root

        try:
            return subprocess.run(
                cmd,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=timeout,
                env=os.environ.copy()
            )
        except subprocess.TimeoutExpired:
            raise Exception(f"Command timed out after {timeout}s: {' '.join(cmd)}")

    def test_legacy_build(self, target: str) -> CITestResult:
        """Test legacy build system."""
        start_time = time.time()
        test_name = f"legacy_build_{target}"

        try:
            self._log(f"Testing legacy build for {target}")

            # Clean previous artifacts
            self._run_command(['scons', '-c'], timeout=30)

            # Build target
            result = self._run_command(['scons', target])

            if result.returncode != 0:
                return CITestResult(
                    test_name=test_name,
                    status='FAIL',
                    duration=time.time() - start_time,
                    message=f"Legacy build failed: {result.stderr[:200]}",
                    details={'stdout': result.stdout, 'stderr': result.stderr}
                )

            # Collect artifacts
            artifacts = self._collect_build_artifacts(target, 'legacy')

            return CITestResult(
                test_name=test_name,
                status='PASS',
                duration=time.time() - start_time,
                message="Legacy build successful",
                artifacts=artifacts
            )

        except Exception as e:
            return CITestResult(
                test_name=test_name,
                status='ERROR',
                duration=time.time() - start_time,
                message=f"Legacy build error: {str(e)}"
            )

    def test_modular_build(self, target: str) -> CITestResult:
        """Test modular build system."""
        start_time = time.time()
        test_name = f"modular_build_{target}"

        try:
            self._log(f"Testing modular build for {target}")

            # Clean previous artifacts
            self._run_command(['scons', '-c', '-f', 'SConscript.modular'], timeout=30)

            # Build target
            result = self._run_command(['scons', '-f', 'SConscript.modular', f'{target}_modular'])

            if result.returncode != 0:
                return CITestResult(
                    test_name=test_name,
                    status='FAIL',
                    duration=time.time() - start_time,
                    message=f"Modular build failed: {result.stderr[:200]}",
                    details={'stdout': result.stdout, 'stderr': result.stderr}
                )

            # Collect artifacts
            artifacts = self._collect_build_artifacts(target, 'modular')

            return CITestResult(
                test_name=test_name,
                status='PASS',
                duration=time.time() - start_time,
                message="Modular build successful",
                artifacts=artifacts
            )

        except Exception as e:
            return CITestResult(
                test_name=test_name,
                status='ERROR',
                duration=time.time() - start_time,
                message=f"Modular build error: {str(e)}"
            )

    def test_binary_equivalence(self, target: str) -> CITestResult:
        """Test that legacy and modular builds produce equivalent binaries."""
        start_time = time.time()
        test_name = f"binary_equivalence_{target}"

        try:
            self._log(f"Testing binary equivalence for {target}")

            # Build both versions
            legacy_result = self.test_legacy_build(target)
            modular_result = self.test_modular_build(target)

            if legacy_result.status != 'PASS' or modular_result.status != 'PASS':
                return CITestResult(
                    test_name=test_name,
                    status='FAIL',
                    duration=time.time() - start_time,
                    message="Cannot compare - one or both builds failed"
                )

            # Compare binary outputs
            differences = []
            expected_outputs = self._get_expected_outputs(target)

            for output_file in expected_outputs:
                legacy_path = self.panda_root / output_file
                modular_path = self.panda_root / output_file.replace(target, f'{target}_modular')

                if not legacy_path.exists() or not modular_path.exists():
                    differences.append(f"Missing output file: {output_file}")
                    continue

                legacy_hash = self._calculate_file_hash(legacy_path)
                modular_hash = self._calculate_file_hash(modular_path)

                if legacy_hash != modular_hash:
                    differences.append(f"{output_file}: hashes differ")

            if differences:
                return CITestResult(
                    test_name=test_name,
                    status='FAIL',
                    duration=time.time() - start_time,
                    message=f"Binary differences found: {', '.join(differences)}",
                    details={'differences': differences}
                )

            return CITestResult(
                test_name=test_name,
                status='PASS',
                duration=time.time() - start_time,
                message="Binaries are equivalent"
            )

        except Exception as e:
            return CITestResult(
                test_name=test_name,
                status='ERROR',
                duration=time.time() - start_time,
                message=f"Binary comparison error: {str(e)}"
            )

    def test_performance_regression(self, target: str) -> CITestResult:
        """Test for performance regressions."""
        start_time = time.time()
        test_name = f"performance_regression_{target}"

        try:
            self._log(f"Testing performance regression for {target}")

            # Time both builds
            legacy_times = []
            modular_times = []

            # Run multiple iterations for statistical validity
            for iteration in range(3):
                self._log(f"Performance test iteration {iteration + 1}/3")

                # Legacy build
                self._run_command(['scons', '-c'], timeout=30)
                legacy_start = time.time()
                result = self._run_command(['scons', target])
                legacy_time = time.time() - legacy_start

                if result.returncode == 0:
                    legacy_times.append(legacy_time)

                # Modular build
                self._run_command(['scons', '-c', '-f', 'SConscript.modular'], timeout=30)
                modular_start = time.time()
                result = self._run_command(['scons', '-f', 'SConscript.modular', f'{target}_modular'])
                modular_time = time.time() - modular_start

                if result.returncode == 0:
                    modular_times.append(modular_time)

            if not legacy_times or not modular_times:
                return CITestResult(
                    test_name=test_name,
                    status='FAIL',
                    duration=time.time() - start_time,
                    message="Could not measure build times - builds failed"
                )

            # Calculate averages
            avg_legacy = sum(legacy_times) / len(legacy_times)
            avg_modular = sum(modular_times) / len(modular_times)

            # Check for regression
            regression_percent = ((avg_modular - avg_legacy) / avg_legacy) * 100

            # Compare with baseline if available
            baseline_key = f"{target}_build_time"
            if baseline_key in self.baseline_data:
                baseline_time = self.baseline_data[baseline_key]
                baseline_regression = ((avg_modular - baseline_time) / baseline_time) * 100
            else:
                baseline_regression = None

            # Update baseline
            self.baseline_data[baseline_key] = avg_modular

            details = {
                'legacy_times': legacy_times,
                'modular_times': modular_times,
                'avg_legacy': avg_legacy,
                'avg_modular': avg_modular,
                'regression_percent': regression_percent,
                'baseline_regression': baseline_regression
            }

            # Determine if this is a regression
            is_regression = False
            message = f"Legacy: {avg_legacy:.2f}s, Modular: {avg_modular:.2f}s"

            if regression_percent > self.config.max_performance_regression_percent:
                is_regression = True
                message += f" (REGRESSION: {regression_percent:.1f}%)"
            elif regression_percent > 0:
                message += f" ({regression_percent:.1f}% slower)"
            else:
                message += f" ({abs(regression_percent):.1f}% faster)"

            if baseline_regression and baseline_regression > self.config.max_performance_regression_percent:
                is_regression = True
                message += f" (Baseline regression: {baseline_regression:.1f}%)"

            status = 'FAIL' if (is_regression and self.config.fail_on_performance_regression) else 'PASS'

            return CITestResult(
                test_name=test_name,
                status=status,
                duration=time.time() - start_time,
                message=message,
                details=details
            )

        except Exception as e:
            return CITestResult(
                test_name=test_name,
                status='ERROR',
                duration=time.time() - start_time,
                message=f"Performance test error: {str(e)}"
            )

    def test_size_regression(self, target: str) -> CITestResult:
        """Test for binary size regressions."""
        start_time = time.time()
        test_name = f"size_regression_{target}"

        try:
            self._log(f"Testing size regression for {target}")

            # Build both versions
            legacy_result = self.test_legacy_build(target)
            modular_result = self.test_modular_build(target)

            if legacy_result.status != 'PASS' or modular_result.status != 'PASS':
                return CITestResult(
                    test_name=test_name,
                    status='FAIL',
                    duration=time.time() - start_time,
                    message="Cannot compare sizes - builds failed"
                )

            # Compare binary sizes
            size_changes = []
            expected_outputs = self._get_expected_outputs(target)

            for output_file in expected_outputs:
                legacy_path = self.panda_root / output_file
                modular_path = self.panda_root / output_file.replace(target, f'{target}_modular')

                if not legacy_path.exists() or not modular_path.exists():
                    continue

                legacy_size = legacy_path.stat().st_size
                modular_size = modular_path.stat().st_size

                size_change = modular_size - legacy_size
                size_change_percent = (size_change / legacy_size) * 100 if legacy_size > 0 else 0

                size_changes.append({
                    'file': output_file,
                    'legacy_size': legacy_size,
                    'modular_size': modular_size,
                    'change_bytes': size_change,
                    'change_percent': size_change_percent
                })

                # Check baseline
                baseline_key = f"{target}_{output_file}_size"
                if baseline_key in self.baseline_data:
                    baseline_size = self.baseline_data[baseline_key]
                    baseline_change = ((modular_size - baseline_size) / baseline_size) * 100
                else:
                    baseline_change = None

                # Update baseline
                self.baseline_data[baseline_key] = modular_size

            # Determine if there's a significant size regression
            max_increase = max((s['change_percent'] for s in size_changes), default=0)
            is_regression = max_increase > self.config.max_size_increase_percent

            message = f"Max size increase: {max_increase:.1f}%"
            status = 'FAIL' if (is_regression and self.config.fail_on_size_increase) else 'PASS'

            return CITestResult(
                test_name=test_name,
                status=status,
                duration=time.time() - start_time,
                message=message,
                details={'size_changes': size_changes}
            )

        except Exception as e:
            return CITestResult(
                test_name=test_name,
                status='ERROR',
                duration=time.time() - start_time,
                message=f"Size test error: {str(e)}"
            )

    def test_incremental_builds(self, target: str) -> CITestResult:
        """Test incremental build functionality."""
        start_time = time.time()
        test_name = f"incremental_builds_{target}"

        try:
            self._log(f"Testing incremental builds for {target}")

            results = {}

            for system in ['legacy', 'modular']:
                script_flag = [] if system == 'legacy' else ['-f', 'SConscript.modular']
                target_name = target if system == 'legacy' else f'{target}_modular'

                # Full build
                self._run_command(['scons', '-c'] + script_flag, timeout=30)
                full_start = time.time()
                result = self._run_command(['scons'] + script_flag + [target_name])
                full_time = time.time() - full_start

                if result.returncode != 0:
                    results[system] = {'error': 'Full build failed'}
                    continue

                # Touch a file and do incremental build
                touch_file = self.panda_root / 'board' / 'health.h'
                if touch_file.exists():
                    touch_file.touch()

                    incremental_start = time.time()
                    result = self._run_command(['scons'] + script_flag + [target_name])
                    incremental_time = time.time() - incremental_start

                    if result.returncode == 0:
                        speedup = full_time / incremental_time if incremental_time > 0 else 0
                        results[system] = {
                            'full_time': full_time,
                            'incremental_time': incremental_time,
                            'speedup': speedup
                        }
                    else:
                        results[system] = {'error': 'Incremental build failed'}

            # Analyze results
            issues = []
            for system, data in results.items():
                if 'error' in data:
                    issues.append(f"{system}: {data['error']}")
                elif data.get('speedup', 0) < 1.5:  # Incremental should be at least 1.5x faster
                    issues.append(f"{system}: incremental build not significantly faster ({data['speedup']:.1f}x)")

            status = 'FAIL' if issues else 'PASS'
            message = "Incremental builds working" if not issues else f"Issues: {', '.join(issues)}"

            return CITestResult(
                test_name=test_name,
                status=status,
                duration=time.time() - start_time,
                message=message,
                details=results
            )

        except Exception as e:
            return CITestResult(
                test_name=test_name,
                status='ERROR',
                duration=time.time() - start_time,
                message=f"Incremental test error: {str(e)}"
            )

    def _get_expected_outputs(self, target: str) -> List[str]:
        """Get expected output files for a target."""
        outputs = {
            'panda_h7': [
                'board/obj/panda_h7/main.bin',
                'board/obj/bootstub.panda_h7.bin',
                'board/obj/panda_h7.bin.signed'
            ],
            'panda_jungle_h7': [
                'board/obj/panda_jungle_h7/main.bin',
                'board/obj/bootstub.panda_jungle_h7.bin',
                'board/obj/panda_jungle_h7.bin.signed'
            ]
        }
        return outputs.get(target, [])

    def _calculate_file_hash(self, filepath: Path) -> str:
        """Calculate SHA256 hash of a file."""
        sha256_hash = hashlib.sha256()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()

    def _collect_build_artifacts(self, target: str, system: str) -> List[str]:
        """Collect build artifacts if archival is enabled."""
        if not self.config.archive_artifacts:
            return []

        artifacts = []
        expected_outputs = self._get_expected_outputs(target)

        for output_file in expected_outputs:
            if system == 'modular':
                output_file = output_file.replace(target, f'{target}_modular')

            source_path = self.panda_root / output_file
            if source_path.exists():
                # Copy to artifacts directory
                artifact_name = f"{system}_{target}_{source_path.name}"
                artifact_path = self.artifacts_dir / artifact_name
                artifact_path.parent.mkdir(parents=True, exist_ok=True)

                import shutil
                shutil.copy2(source_path, artifact_path)
                artifacts.append(str(artifact_path))

        return artifacts

    def _send_notification(self, report: Dict[str, Any]):
        """Send notification if webhook is configured."""
        if not self.config.notification_webhook:
            return

        try:
            # Prepare notification payload
            status = report['overall_status']
            payload = {
                'text': f"Panda CI Validation: {status}",
                'attachments': [
                    {
                        'color': 'good' if status == 'PASS' else 'danger',
                        'fields': [
                            {
                                'title': 'Tests',
                                'value': f"{report['passed_tests']}/{report['total_tests']} passed",
                                'short': True
                            },
                            {
                                'title': 'Duration',
                                'value': f"{report['total_duration']:.1f}s",
                                'short': True
                            }
                        ]
                    }
                ]
            }

            response = requests.post(
                self.config.notification_webhook,
                json=payload,
                timeout=10
            )
            response.raise_for_status()
            self._log("Notification sent successfully")

        except Exception as e:
            self._log(f"Failed to send notification: {e}")

    def run_validation_suite(self, targets: List[str] = None) -> Dict[str, Any]:
        """Run the complete CI validation suite."""
        start_time = time.time()

        if targets is None:
            targets = ['panda_h7']  # Default target for CI

        self._log("Starting CI validation pipeline")
        self._log(f"Targets: {', '.join(targets)}")
        self._log(f"Config: {asdict(self.config)}")

        # Define test matrix
        test_functions = []

        for target in targets:
            test_functions.extend([
                (self.test_legacy_build, target),
                (self.test_modular_build, target),
                (self.test_binary_equivalence, target),
            ])

            if self.config.enable_performance_tests:
                test_functions.append((self.test_performance_regression, target))
                test_functions.append((self.test_size_regression, target))

            if self.config.enable_incremental_tests:
                test_functions.append((self.test_incremental_builds, target))

        # Run tests in parallel
        with ThreadPoolExecutor(max_workers=min(self.config.parallel_jobs, len(test_functions))) as executor:
            # Submit all tests
            future_to_test = {
                executor.submit(test_func, *args): (test_func.__name__, args)
                for test_func, *args in test_functions
            }

            # Collect results
            for future in as_completed(future_to_test):
                test_name, args = future_to_test[future]
                try:
                    result = future.result()
                    self._add_test_result(result)
                    status_symbol = "✓" if result.status == 'PASS' else "✗"
                    self._log(f"{status_symbol} {result.test_name}: {result.status} ({result.duration:.1f}s)")
                except Exception as e:
                    error_result = CITestResult(
                        test_name=f"{test_name}_{args[0] if args else 'unknown'}",
                        status='ERROR',
                        duration=0,
                        message=f"Test execution error: {str(e)}"
                    )
                    self._add_test_result(error_result)
                    self._log(f"✗ {error_result.test_name}: ERROR - {str(e)}")

        # Generate final report
        total_duration = time.time() - start_time
        passed_tests = sum(1 for r in self.test_results if r.status == 'PASS')
        total_tests = len(self.test_results)
        overall_status = 'PASS' if passed_tests == total_tests else 'FAIL'

        report = {
            'timestamp': time.time(),
            'overall_status': overall_status,
            'total_tests': total_tests,
            'passed_tests': passed_tests,
            'failed_tests': total_tests - passed_tests,
            'total_duration': total_duration,
            'test_results': [asdict(r) for r in self.test_results],
            'config': asdict(self.config),
            'baseline_updated': True
        }

        # Save baseline data
        self._save_baseline_data(self.baseline_data)

        # Archive artifacts
        if self.config.archive_artifacts:
            self._create_artifact_archive()

        # Save report
        report_file = self.artifacts_dir / 'ci_validation_report.json'
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)

        # Send notification
        self._send_notification(report)

        # Print summary
        self._print_ci_summary(report)

        return report

    def _create_artifact_archive(self):
        """Create a ZIP archive of all artifacts."""
        archive_path = self.artifacts_dir / f'build_artifacts_{int(time.time())}.zip'

        with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk(self.artifacts_dir):
                for file in files:
                    if file.endswith('.zip'):
                        continue  # Don't include other archives
                    file_path = Path(root) / file
                    arc_name = file_path.relative_to(self.artifacts_dir)
                    zipf.write(file_path, arc_name)

        self._log(f"Artifacts archived to: {archive_path}")

    def _print_ci_summary(self, report: Dict[str, Any]):
        """Print CI-friendly summary."""
        print(f"\n{'='*60}")
        print("CI VALIDATION SUMMARY")
        print(f"{'='*60}")

        status_symbol = "✓" if report['overall_status'] == 'PASS' else "✗"
        print(f"{status_symbol} Overall Status: {report['overall_status']}")
        print(f"Tests: {report['passed_tests']}/{report['total_tests']} passed")
        print(f"Duration: {report['total_duration']:.1f}s")

        # Show failed tests
        failed_tests = [r for r in self.test_results if r['status'] != 'PASS']
        if failed_tests:
            print(f"\nFailed Tests:")
            for test in failed_tests:
                print(f"  ✗ {test['test_name']}: {test['status']} - {test['message']}")

        print(f"\nReport: {self.artifacts_dir / 'ci_validation_report.json'}")


def main():
    """Main entry point for CI validation."""
    import argparse

    parser = argparse.ArgumentParser(description='Panda CI Validation Pipeline')
    parser.add_argument('--targets', nargs='+', default=['panda_h7'],
                       help='Targets to test')
    parser.add_argument('--no-performance', action='store_true',
                       help='Skip performance tests')
    parser.add_argument('--no-incremental', action='store_true',
                       help='Skip incremental build tests')
    parser.add_argument('--archive', action='store_true',
                       help='Archive build artifacts')
    parser.add_argument('--webhook',
                       help='Notification webhook URL')

    args = parser.parse_args()

    # Configure validation
    config = CIValidationConfig(
        enable_performance_tests=not args.no_performance,
        enable_incremental_tests=not args.no_incremental,
        archive_artifacts=args.archive,
        notification_webhook=args.webhook
    )

    # Run validation
    pipeline = CIValidationPipeline(config)
    report = pipeline.run_validation_suite(args.targets)

    # Exit with appropriate code
    exit_code = 0 if report['overall_status'] == 'PASS' else 1
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
