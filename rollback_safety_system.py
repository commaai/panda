#!/usr/bin/env python3
"""
Rollback and Safety System for Panda Modular Migration

This system provides comprehensive safety mechanisms for the migration from
legacy to modular build system, including:

1. Automated rollback capabilities
2. System state management
3. Build verification before deployment
4. Configuration backup and restore
5. Emergency recovery procedures
6. Compatibility validation
7. Migration progress tracking
8. Safe deployment protocols

Features:
- Zero-downtime rollback mechanisms
- Comprehensive state snapshots
- Automated safety checks
- Emergency stop procedures
- Build system coexistence validation
- Migration checkpoint system
"""

import os
import sys
import json
import time
import hashlib
import subprocess
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field, asdict
from datetime import datetime
import tarfile
import threading


@dataclass
class SystemSnapshot:
    """Snapshot of the build system state."""
    timestamp: float
    snapshot_id: str
    description: str
    files_backup: str  # Path to backup archive
    build_configs: Dict[str, Any] = field(default_factory=dict)
    environment_state: Dict[str, str] = field(default_factory=dict)
    build_outputs: Dict[str, str] = field(default_factory=dict)  # file -> hash
    validation_results: Dict[str, bool] = field(default_factory=dict)
    migration_stage: str = "unknown"


@dataclass
class SafetyCheck:
    """Definition of a safety check."""
    name: str
    description: str
    critical: bool  # If true, failure blocks deployment
    timeout: int = 60  # seconds
    retry_count: int = 1


@dataclass
class MigrationCheckpoint:
    """Migration progress checkpoint."""
    stage: str
    description: str
    timestamp: float
    successful: bool
    snapshot_id: Optional[str] = None
    validation_results: Dict[str, bool] = field(default_factory=dict)
    rollback_instructions: List[str] = field(default_factory=list)


class RollbackSafetySystem:
    """Main rollback and safety management system."""

    def __init__(self, panda_root: str = None):
        self.panda_root = Path(panda_root or os.getcwd())
        self.safety_dir = self.panda_root / '.modular_migration_safety'
        self.safety_dir.mkdir(exist_ok=True)

        # Safety configuration
        self.snapshots_dir = self.safety_dir / 'snapshots'
        self.snapshots_dir.mkdir(exist_ok=True)

        self.backups_dir = self.safety_dir / 'backups'
        self.backups_dir.mkdir(exist_ok=True)

        # State tracking
        self.state_file = self.safety_dir / 'migration_state.json'
        self.checkpoints_file = self.safety_dir / 'migration_checkpoints.json'

        # Load existing state
        self.migration_state = self._load_migration_state()
        self.checkpoints = self._load_checkpoints()

        # Define safety checks
        self.safety_checks = self._define_safety_checks()

        # Lock for thread safety
        self._lock = threading.Lock()

    def _load_migration_state(self) -> Dict[str, Any]:
        """Load current migration state."""
        if self.state_file.exists():
            try:
                with open(self.state_file, 'r') as f:
                    return json.load(f)
            except Exception as e:
                print(f"Warning: Could not load migration state: {e}")

        return {
            'current_stage': 'legacy',
            'last_snapshot_id': None,
            'emergency_rollback_enabled': True,
            'safety_checks_enabled': True,
            'coexistence_mode': True
        }

    def _save_migration_state(self):
        """Save current migration state."""
        with open(self.state_file, 'w') as f:
            json.dump(self.migration_state, f, indent=2)

    def _load_checkpoints(self) -> List[MigrationCheckpoint]:
        """Load migration checkpoints."""
        if self.checkpoints_file.exists():
            try:
                with open(self.checkpoints_file, 'r') as f:
                    data = json.load(f)
                    return [MigrationCheckpoint(**cp) for cp in data]
            except Exception as e:
                print(f"Warning: Could not load checkpoints: {e}")
        return []

    def _save_checkpoints(self):
        """Save migration checkpoints."""
        data = [asdict(cp) for cp in self.checkpoints]
        with open(self.checkpoints_file, 'w') as f:
            json.dump(data, f, indent=2)

    def _define_safety_checks(self) -> List[SafetyCheck]:
        """Define the standard safety checks."""
        return [
            SafetyCheck(
                name="legacy_build_functional",
                description="Verify legacy build system works",
                critical=True,
                timeout=300
            ),
            SafetyCheck(
                name="modular_build_functional",
                description="Verify modular build system works",
                critical=True,
                timeout=300
            ),
            SafetyCheck(
                name="binary_equivalence",
                description="Verify legacy and modular produce equivalent binaries",
                critical=True,
                timeout=600
            ),
            SafetyCheck(
                name="incremental_builds",
                description="Verify incremental builds work",
                critical=False,
                timeout=300
            ),
            SafetyCheck(
                name="performance_regression",
                description="Check for performance regressions",
                critical=False,
                timeout=600
            ),
            SafetyCheck(
                name="dependency_validation",
                description="Validate dependency structure",
                critical=False,
                timeout=120
            ),
            SafetyCheck(
                name="file_integrity",
                description="Check file integrity and permissions",
                critical=True,
                timeout=60
            ),
            SafetyCheck(
                name="environment_compatibility",
                description="Verify environment compatibility",
                critical=True,
                timeout=60
            )
        ]

    def create_system_snapshot(self, description: str, stage: str = "unknown") -> SystemSnapshot:
        """Create a complete snapshot of the current system state."""
        print(f"Creating system snapshot: {description}")

        timestamp = time.time()
        snapshot_id = f"snapshot_{int(timestamp)}_{stage}"

        # Create snapshot directory
        snapshot_dir = self.snapshots_dir / snapshot_id
        snapshot_dir.mkdir(exist_ok=True)

        # Files to backup
        critical_files = [
            'SConscript',
            'SConscript.modular',
            'SConscript.incremental',
            'SConstruct',
            'modules/',
            'board/',
            'crypto/'
        ]

        # Create backup archive
        backup_archive = self.backups_dir / f"{snapshot_id}.tar.gz"

        with tarfile.open(backup_archive, 'w:gz') as tar:
            for file_pattern in critical_files:
                file_path = self.panda_root / file_pattern
                if file_path.exists():
                    # Add to archive with relative path
                    arcname = file_path.relative_to(self.panda_root)
                    tar.add(file_path, arcname=arcname)

        # Capture build configurations
        build_configs = self._capture_build_configs()

        # Capture environment state
        env_state = self._capture_environment_state()

        # Capture current build outputs
        build_outputs = self._capture_build_outputs()

        # Run validation checks
        validation_results = self._run_validation_checks()

        snapshot = SystemSnapshot(
            timestamp=timestamp,
            snapshot_id=snapshot_id,
            description=description,
            files_backup=str(backup_archive),
            build_configs=build_configs,
            environment_state=env_state,
            build_outputs=build_outputs,
            validation_results=validation_results,
            migration_stage=stage
        )

        # Save snapshot metadata
        snapshot_file = snapshot_dir / 'snapshot.json'
        with open(snapshot_file, 'w') as f:
            json.dump(asdict(snapshot), f, indent=2)

        print(f"✓ Snapshot created: {snapshot_id}")
        return snapshot

    def _capture_build_configs(self) -> Dict[str, Any]:
        """Capture current build configuration."""
        configs = {}

        # SCons configuration
        scons_files = ['SConscript', 'SConscript.modular', 'SConstruct']
        for scons_file in scons_files:
            file_path = self.panda_root / scons_file
            if file_path.exists():
                configs[scons_file] = {
                    'exists': True,
                    'size': file_path.stat().st_size,
                    'mtime': file_path.stat().st_mtime,
                    'hash': self._calculate_file_hash(file_path)
                }

        # Module configuration
        module_registry = self.panda_root / 'modules' / 'module_registry.py'
        if module_registry.exists():
            configs['module_registry'] = {
                'exists': True,
                'hash': self._calculate_file_hash(module_registry)
            }

        return configs

    def _capture_environment_state(self) -> Dict[str, str]:
        """Capture relevant environment variables."""
        important_env_vars = [
            'PATH', 'PYTHONPATH', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS',
            'RELEASE', 'DEBUG', 'CERT'
        ]

        env_state = {}
        for var in important_env_vars:
            env_state[var] = os.environ.get(var, '')

        return env_state

    def _capture_build_outputs(self) -> Dict[str, str]:
        """Capture hashes of current build outputs."""
        outputs = {}

        obj_dir = self.panda_root / 'board' / 'obj'
        if obj_dir.exists():
            for output_file in obj_dir.rglob('*.bin'):
                rel_path = output_file.relative_to(self.panda_root)
                outputs[str(rel_path)] = self._calculate_file_hash(output_file)

        return outputs

    def _run_validation_checks(self) -> Dict[str, bool]:
        """Run basic validation checks."""
        results = {}

        # Check file existence
        critical_files = ['SConscript', 'board/main.c', 'crypto/rsa.c']
        for file_path in critical_files:
            full_path = self.panda_root / file_path
            results[f"file_exists_{file_path}"] = full_path.exists()

        # Check for obvious corruption
        try:
            # Try to parse main SConscript
            sconscript = self.panda_root / 'SConscript'
            if sconscript.exists():
                content = sconscript.read_text()
                results['sconscript_parseable'] = 'def build_project' in content
        except Exception:
            results['sconscript_parseable'] = False

        return results

    def _calculate_file_hash(self, file_path: Path) -> str:
        """Calculate SHA256 hash of a file."""
        sha256_hash = hashlib.sha256()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()

    def rollback_to_snapshot(self, snapshot_id: str) -> bool:
        """Rollback system to a previous snapshot."""
        print(f"Rolling back to snapshot: {snapshot_id}")

        # Find snapshot
        snapshot_dir = self.snapshots_dir / snapshot_id
        snapshot_file = snapshot_dir / 'snapshot.json'

        if not snapshot_file.exists():
            print(f"✗ Snapshot not found: {snapshot_id}")
            return False

        try:
            # Load snapshot metadata
            with open(snapshot_file, 'r') as f:
                snapshot_data = json.load(f)

            snapshot = SystemSnapshot(**snapshot_data)

            # Create current state backup before rollback
            pre_rollback_snapshot = self.create_system_snapshot(
                f"Pre-rollback backup before restoring {snapshot_id}",
                "pre_rollback"
            )

            # Restore files from backup
            backup_archive = Path(snapshot.files_backup)
            if not backup_archive.exists():
                print(f"✗ Backup archive not found: {backup_archive}")
                return False

            print("Restoring files from backup...")
            with tarfile.open(backup_archive, 'r:gz') as tar:
                tar.extractall(self.panda_root)

            # Verify restoration
            verification_passed = self._verify_rollback(snapshot)

            if verification_passed:
                # Update migration state
                self.migration_state['current_stage'] = snapshot.migration_stage
                self.migration_state['last_snapshot_id'] = snapshot_id
                self._save_migration_state()

                # Add checkpoint
                checkpoint = MigrationCheckpoint(
                    stage="rollback",
                    description=f"Rolled back to {snapshot_id}",
                    timestamp=time.time(),
                    successful=True,
                    snapshot_id=pre_rollback_snapshot.snapshot_id
                )
                self.checkpoints.append(checkpoint)
                self._save_checkpoints()

                print(f"✓ Successfully rolled back to {snapshot_id}")
                return True
            else:
                print(f"✗ Rollback verification failed for {snapshot_id}")
                return False

        except Exception as e:
            print(f"✗ Rollback failed: {e}")
            return False

    def _verify_rollback(self, snapshot: SystemSnapshot) -> bool:
        """Verify that rollback was successful."""
        print("Verifying rollback...")

        # Check critical files exist
        for config_name, config_data in snapshot.build_configs.items():
            if config_data.get('exists'):
                file_path = self.panda_root / config_name
                if not file_path.exists():
                    print(f"✗ Missing file after rollback: {config_name}")
                    return False

                # Verify hash if available
                if 'hash' in config_data:
                    current_hash = self._calculate_file_hash(file_path)
                    if current_hash != config_data['hash']:
                        print(f"✗ File hash mismatch after rollback: {config_name}")
                        return False

        # Try a simple build to verify functionality
        try:
            result = subprocess.run(
                ['scons', '--dry-run', 'panda_h7'],
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                print("✗ Build system not functional after rollback")
                return False

        except Exception as e:
            print(f"✗ Could not verify build system: {e}")
            return False

        print("✓ Rollback verification passed")
        return True

    def run_safety_checks(self, checks: List[str] = None) -> Dict[str, bool]:
        """Run comprehensive safety checks."""
        print("Running safety checks...")

        if checks is None:
            checks_to_run = self.safety_checks
        else:
            checks_to_run = [c for c in self.safety_checks if c.name in checks]

        results = {}
        critical_failures = []

        for check in checks_to_run:
            print(f"  Running: {check.description}")

            try:
                success = self._execute_safety_check(check)
                results[check.name] = success

                if not success:
                    if check.critical:
                        critical_failures.append(check.name)
                    print(f"    ✗ {check.name} FAILED")
                else:
                    print(f"    ✓ {check.name} PASSED")

            except Exception as e:
                results[check.name] = False
                if check.critical:
                    critical_failures.append(check.name)
                print(f"    ✗ {check.name} ERROR: {e}")

        # Summary
        passed = sum(1 for r in results.values() if r)
        total = len(results)

        print(f"\nSafety check summary: {passed}/{total} passed")

        if critical_failures:
            print(f"CRITICAL FAILURES: {', '.join(critical_failures)}")
            print("⚠ DEPLOYMENT BLOCKED BY CRITICAL FAILURES")

        return results

    def _execute_safety_check(self, check: SafetyCheck) -> bool:
        """Execute a specific safety check."""
        if check.name == "legacy_build_functional":
            return self._check_legacy_build()
        elif check.name == "modular_build_functional":
            return self._check_modular_build()
        elif check.name == "binary_equivalence":
            return self._check_binary_equivalence()
        elif check.name == "incremental_builds":
            return self._check_incremental_builds()
        elif check.name == "performance_regression":
            return self._check_performance_regression()
        elif check.name == "dependency_validation":
            return self._check_dependency_validation()
        elif check.name == "file_integrity":
            return self._check_file_integrity()
        elif check.name == "environment_compatibility":
            return self._check_environment_compatibility()
        else:
            return False

    def _check_legacy_build(self) -> bool:
        """Check if legacy build system works."""
        try:
            result = subprocess.run(
                ['scons', '-c'],
                cwd=self.panda_root,
                capture_output=True,
                timeout=30
            )

            result = subprocess.run(
                ['scons', 'panda_h7'],
                cwd=self.panda_root,
                capture_output=True,
                timeout=300
            )

            return result.returncode == 0
        except Exception:
            return False

    def _check_modular_build(self) -> bool:
        """Check if modular build system works."""
        modular_script = self.panda_root / 'SConscript.modular'
        if not modular_script.exists():
            return False

        try:
            result = subprocess.run(
                ['scons', '-c', '-f', 'SConscript.modular'],
                cwd=self.panda_root,
                capture_output=True,
                timeout=30
            )

            result = subprocess.run(
                ['scons', '-f', 'SConscript.modular', 'panda_h7_modular'],
                cwd=self.panda_root,
                capture_output=True,
                timeout=300
            )

            return result.returncode == 0
        except Exception:
            return False

    def _check_binary_equivalence(self) -> bool:
        """Check if legacy and modular builds produce equivalent binaries."""
        try:
            # Run comparison tool
            result = subprocess.run(
                [sys.executable, 'build_comparison_pipeline.py', '--target', 'panda_h7'],
                cwd=self.panda_root,
                capture_output=True,
                timeout=600
            )

            return result.returncode == 0
        except Exception:
            return False

    def _check_incremental_builds(self) -> bool:
        """Check incremental build functionality."""
        try:
            # Full build
            subprocess.run(['scons', '-c'], cwd=self.panda_root, timeout=30)
            result1 = subprocess.run(['scons', 'panda_h7'], cwd=self.panda_root, timeout=300)
            if result1.returncode != 0:
                return False

            # Touch a file and rebuild
            touch_file = self.panda_root / 'board' / 'health.h'
            if touch_file.exists():
                touch_file.touch()
                result2 = subprocess.run(['scons', 'panda_h7'], cwd=self.panda_root, timeout=60)
                return result2.returncode == 0

            return True
        except Exception:
            return False

    def _check_performance_regression(self) -> bool:
        """Check for significant performance regressions."""
        try:
            # Run performance analysis
            result = subprocess.run(
                [sys.executable, 'performance_analysis_suite.py', '--profile-builds'],
                cwd=self.panda_root,
                capture_output=True,
                timeout=600
            )

            # For safety check, we just verify it runs without crashing
            return result.returncode == 0
        except Exception:
            return False

    def _check_dependency_validation(self) -> bool:
        """Check dependency structure is valid."""
        try:
            # Check module registry functionality
            sys.path.insert(0, str(self.panda_root / 'modules'))
            from module_registry import ModuleRegistry

            registry = ModuleRegistry()
            registry.validate_all_dependencies()
            return True
        except Exception:
            return False

    def _check_file_integrity(self) -> bool:
        """Check file integrity and permissions."""
        critical_files = [
            'SConscript',
            'board/main.c',
            'crypto/rsa.c',
            'crypto/sha.c'
        ]

        for file_path in critical_files:
            full_path = self.panda_root / file_path
            if not full_path.exists():
                return False

            # Check basic readability
            try:
                with open(full_path, 'r') as f:
                    f.read(100)  # Read first 100 chars
            except Exception:
                return False

        return True

    def _check_environment_compatibility(self) -> bool:
        """Check environment compatibility."""
        # Check required tools
        required_tools = ['scons', 'arm-none-eabi-gcc']

        for tool in required_tools:
            try:
                result = subprocess.run(
                    ['which', tool],
                    capture_output=True,
                    timeout=10
                )
                if result.returncode != 0:
                    return False
            except Exception:
                return False

        return True

    def emergency_rollback(self) -> bool:
        """Emergency rollback to last known good state."""
        print("🚨 EMERGENCY ROLLBACK INITIATED")

        if not self.migration_state.get('emergency_rollback_enabled'):
            print("✗ Emergency rollback is disabled")
            return False

        # Find last successful checkpoint
        successful_checkpoints = [cp for cp in self.checkpoints if cp.successful]
        if not successful_checkpoints:
            print("✗ No successful checkpoints found")
            return False

        last_checkpoint = successful_checkpoints[-1]

        if last_checkpoint.snapshot_id:
            print(f"Rolling back to checkpoint: {last_checkpoint.stage}")
            return self.rollback_to_snapshot(last_checkpoint.snapshot_id)
        else:
            print("✗ No snapshot associated with last checkpoint")
            return False

    def create_migration_checkpoint(self, stage: str, description: str) -> MigrationCheckpoint:
        """Create a migration checkpoint."""
        print(f"Creating checkpoint: {stage}")

        # Create snapshot
        snapshot = self.create_system_snapshot(
            f"Checkpoint: {description}",
            stage
        )

        # Run safety checks
        safety_results = self.run_safety_checks()

        # Determine if checkpoint is successful
        critical_checks = [c.name for c in self.safety_checks if c.critical]
        critical_passed = all(safety_results.get(check, False) for check in critical_checks)

        checkpoint = MigrationCheckpoint(
            stage=stage,
            description=description,
            timestamp=time.time(),
            successful=critical_passed,
            snapshot_id=snapshot.snapshot_id,
            validation_results=safety_results,
            rollback_instructions=[
                f"python rollback_safety_system.py --rollback {snapshot.snapshot_id}",
                f"Restore from snapshot created at {datetime.fromtimestamp(snapshot.timestamp)}"
            ]
        )

        self.checkpoints.append(checkpoint)
        self._save_checkpoints()

        # Update migration state
        self.migration_state['current_stage'] = stage
        self.migration_state['last_snapshot_id'] = snapshot.snapshot_id
        self._save_migration_state()

        status = "✓ SUCCESSFUL" if critical_passed else "✗ FAILED"
        print(f"Checkpoint created: {status}")

        return checkpoint

    def get_migration_status(self) -> Dict[str, Any]:
        """Get current migration status and recommendations."""
        # Analyze checkpoints
        total_checkpoints = len(self.checkpoints)
        successful_checkpoints = sum(1 for cp in self.checkpoints if cp.successful)

        # Recent checkpoint status
        recent_checkpoint = self.checkpoints[-1] if self.checkpoints else None

        # Safety status
        last_safety_check = None
        if recent_checkpoint:
            last_safety_check = recent_checkpoint.validation_results

        status = {
            'current_stage': self.migration_state.get('current_stage', 'unknown'),
            'total_checkpoints': total_checkpoints,
            'successful_checkpoints': successful_checkpoints,
            'last_checkpoint': {
                'stage': recent_checkpoint.stage if recent_checkpoint else None,
                'successful': recent_checkpoint.successful if recent_checkpoint else None,
                'timestamp': recent_checkpoint.timestamp if recent_checkpoint else None
            } if recent_checkpoint else None,
            'emergency_rollback_available': bool(self.migration_state.get('last_snapshot_id')),
            'safety_status': self._analyze_safety_status(last_safety_check),
            'recommendations': self._get_migration_recommendations()
        }

        return status

    def _analyze_safety_status(self, safety_results: Dict[str, bool]) -> Dict[str, Any]:
        """Analyze safety check results."""
        if not safety_results:
            return {'status': 'unknown', 'message': 'No safety checks run'}

        critical_checks = [c.name for c in self.safety_checks if c.critical]
        critical_failures = [check for check in critical_checks
                           if not safety_results.get(check, False)]

        total_checks = len(safety_results)
        passed_checks = sum(1 for r in safety_results.values() if r)

        if critical_failures:
            status = 'critical_failure'
            message = f"Critical failures: {', '.join(critical_failures)}"
        elif passed_checks == total_checks:
            status = 'all_passed'
            message = "All safety checks passed"
        else:
            status = 'partial_failure'
            message = f"{passed_checks}/{total_checks} checks passed"

        return {
            'status': status,
            'message': message,
            'passed_checks': passed_checks,
            'total_checks': total_checks,
            'critical_failures': critical_failures
        }

    def _get_migration_recommendations(self) -> List[str]:
        """Get recommendations for next migration steps."""
        recommendations = []

        current_stage = self.migration_state.get('current_stage', 'unknown')

        if current_stage == 'legacy':
            recommendations.extend([
                "Create initial checkpoint before starting migration",
                "Run comprehensive safety checks",
                "Verify build system coexistence"
            ])
        elif current_stage == 'coexistence':
            recommendations.extend([
                "Validate modular build produces equivalent outputs",
                "Test incremental builds in both systems",
                "Begin module-by-module migration"
            ])
        elif current_stage == 'partial_migration':
            recommendations.extend([
                "Continue migrating remaining modules",
                "Monitor performance metrics",
                "Validate each module migration"
            ])
        elif current_stage == 'modular':
            recommendations.extend([
                "Run full validation suite",
                "Performance optimization if needed",
                "Consider deprecating legacy build system"
            ])

        # Recent failures
        if self.checkpoints:
            recent_checkpoint = self.checkpoints[-1]
            if not recent_checkpoint.successful:
                recommendations.insert(0, "Address safety check failures before proceeding")

        return recommendations

    def print_status_report(self):
        """Print comprehensive status report."""
        status = self.get_migration_status()

        print(f"\n{'='*80}")
        print("MIGRATION SAFETY STATUS REPORT")
        print(f"{'='*80}")

        print("\nCURRENT STATE:")
        print(f"  Stage: {status['current_stage']}")
        print(f"  Checkpoints: {status['successful_checkpoints']}/{status['total_checkpoints']} successful")
        print(f"  Emergency rollback: {'✓ Available' if status['emergency_rollback_available'] else '✗ Not available'}")

        if status['last_checkpoint']:
            cp = status['last_checkpoint']
            timestamp = datetime.fromtimestamp(cp['timestamp']).strftime('%Y-%m-%d %H:%M:%S')
            status_icon = "✓" if cp['successful'] else "✗"
            print(f"  Last checkpoint: {status_icon} {cp['stage']} ({timestamp})")

        safety = status['safety_status']
        print("\nSAFETY STATUS:")
        print(f"  Overall: {safety['status'].replace('_', ' ').title()}")
        print(f"  Message: {safety['message']}")

        if safety.get('critical_failures'):
            print(f"  Critical failures: {', '.join(safety['critical_failures'])}")

        print("\nRECOMMENDATIONS:")
        for i, rec in enumerate(status['recommendations'], 1):
            print(f"  {i}. {rec}")

        print("\nSAFETY SYSTEM STATUS:")
        print(f"  Snapshots directory: {self.snapshots_dir}")
        print(f"  Backups directory: {self.backups_dir}")
        print(f"  Total snapshots: {len(list(self.snapshots_dir.glob('*')))}")


def main():
    """Main entry point for rollback safety system."""
    import argparse

    parser = argparse.ArgumentParser(description='Panda Rollback Safety System')
    parser.add_argument('--create-snapshot', metavar='DESCRIPTION',
                       help='Create system snapshot')
    parser.add_argument('--rollback', metavar='SNAPSHOT_ID',
                       help='Rollback to snapshot')
    parser.add_argument('--emergency-rollback', action='store_true',
                       help='Emergency rollback to last good state')
    parser.add_argument('--safety-checks', action='store_true',
                       help='Run safety checks')
    parser.add_argument('--create-checkpoint', metavar='STAGE',
                       help='Create migration checkpoint')
    parser.add_argument('--status', action='store_true',
                       help='Show migration status')
    parser.add_argument('--stage', default='manual',
                       help='Migration stage for snapshots/checkpoints')

    args = parser.parse_args()

    safety_system = RollbackSafetySystem()

    if args.create_snapshot:
        safety_system.create_system_snapshot(args.create_snapshot, args.stage)

    elif args.rollback:
        success = safety_system.rollback_to_snapshot(args.rollback)
        return 0 if success else 1

    elif args.emergency_rollback:
        success = safety_system.emergency_rollback()
        return 0 if success else 1

    elif args.safety_checks:
        results = safety_system.run_safety_checks()
        critical_checks = [c.name for c in safety_system.safety_checks if c.critical]
        critical_passed = all(results.get(check, False) for check in critical_checks)
        return 0 if critical_passed else 1

    elif args.create_checkpoint:
        checkpoint = safety_system.create_migration_checkpoint(
            args.create_checkpoint,
            f"Manual checkpoint for {args.create_checkpoint} stage"
        )
        return 0 if checkpoint.successful else 1

    elif args.status:
        safety_system.print_status_report()

    else:
        safety_system.print_status_report()

    return 0


if __name__ == "__main__":
    sys.exit(main())